// server.cpp -- Workbench loopback HTTP server.
// Security: loopback-only bind, Host header validation, token cookie auth,
// path-traversal rejection, CSP headers. No TLS (localhost only).
#include "workbench/server.hpp"
#include "workbench/token.hpp"
#include "workbench/version.hpp"
#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable: 4127 4244 4456 4702 4996)
#endif
#include "httplib.h"
#ifdef _MSC_VER
#  pragma warning(pop)
#endif
#include "json.hpp"
#include "workbench/job_manager.hpp"
#include "workbench/fem_routes.hpp"
#include "workbench/file_store.hpp"
#include "workbench/file_routes.hpp"
#include "workbench/device_routes.hpp"
#include "workbench/platform.hpp"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>

namespace workbench {
namespace {

const std::unordered_map<std::string,std::string> kMimeTypes{
    {".html","text/html; charset=utf-8"},{".js","application/javascript; charset=utf-8"},
    {".mjs","application/javascript; charset=utf-8"},{".css","text/css; charset=utf-8"},
    {".json","application/json"},{".png","image/png"},{".jpg","image/jpeg"},
    {".svg","image/svg+xml"},{".ico","image/x-icon"},{".woff","font/woff"},
    {".woff2","font/woff2"},{".txt","text/plain; charset=utf-8"},
};

std::string mime_for(const std::string& path) {
    const auto pos = path.rfind('.');
    if (pos == std::string::npos) return "application/octet-stream";
    const auto it = kMimeTypes.find(path.substr(pos));
    return it != kMimeTypes.end() ? it->second : "application/octet-stream";
}

bool path_is_safe(const std::string& p) noexcept {
    std::string seg;
    for (const char c : p) {
        if (c == '/') { if (seg == "..") return false; seg.clear(); }
        else           { seg += c; }
    }
    return seg != "..";
}

// CSP: default-src 'self' blocks everything; img/worker-src for Three.js/Plotly;
// style unsafe-inline for Tailwind; frame-ancestors/form-action/object-src lock-down.
void add_security_headers(httplib::Response& res) {
    res.set_header("X-Content-Type-Options", "nosniff");
    res.set_header("X-Frame-Options",        "DENY");
    res.set_header("Referrer-Policy",        "same-origin");
    res.set_header("Content-Security-Policy",
        "default-src 'self'; img-src 'self' data: blob:; worker-src blob:; "
        "style-src 'self' 'unsafe-inline'; font-src 'self'; connect-src 'self'; "
        "frame-ancestors 'none'; base-uri 'none'; form-action 'self'; object-src 'none'");
}

}  // namespace

struct Server::Impl {
    std::unique_ptr<JobManager>           job_manager;
    FileStore                             file_store;
    DeviceRegistry                        device_registry;
    httplib::Server                       svr;
    std::thread                           listener_thread;
    std::chrono::steady_clock::time_point start_time;
};

Server::Server(ServerConfig cfg) : cfg_(std::move(cfg)) { impl_ = new Impl{}; }
Server::~Server() { stop(); delete impl_; }

uint16_t Server::start() {
    impl_->start_time = std::chrono::steady_clock::now();
    auto& svr = impl_->svr;

    // Pre-routing: Host header validation + cookie auth for /api/*.
    // Must run before any route handler.
    svr.set_pre_routing_handler(
        [this](const httplib::Request& req, httplib::Response& res)
            -> httplib::Server::HandlerResponse {
            add_security_headers(res);
            const std::string expected = "127.0.0.1:" + std::to_string(port_);
            if (req.get_header_value("Host") != expected) {
                res.status = 400;
                res.set_content("Bad Host header", "text/plain");
                return httplib::Server::HandlerResponse::Handled;
            }
            // Cookie check for /api/* paths (skip in dev mode).
            if (!cfg_.dev_mode && req.path.rfind("/api/", 0) == 0) {
                const std::string needle = "wb_session=" + cfg_.token;
                if (req.get_header_value("Cookie").find(needle) == std::string::npos) {
                    res.status = 401;
                    res.set_content("Unauthorized", "text/plain");
                    return httplib::Server::HandlerResponse::Handled;
                }
            }
            return httplib::Server::HandlerResponse::Unhandled;
        });

    // Token exchange: GET /?t=TOKEN -> set HttpOnly cookie -> redirect to /.
    if (!cfg_.dev_mode) {
        svr.Get("/", [this](const httplib::Request& req, httplib::Response& res) {
            if (!req.has_param("t")) return; // no token param -> fall through to static
            if (req.get_param_value("t") != cfg_.token) {
                res.status = 403;
                res.set_content("Invalid token", "text/plain");
                return;
            }
            res.set_header("Set-Cookie",
                "wb_session=" + cfg_.token + "; Path=/; HttpOnly; SameSite=Strict");
            res.set_redirect("/");
        });
    }

    // GET /api/health
    svr.Get("/api/health", [this](const httplib::Request&, httplib::Response& res) {
        using namespace std::chrono;
        const auto uptime = duration_cast<milliseconds>(
            steady_clock::now() - impl_->start_time).count();
        const int64_t ltr = launch_to_ready_ms_.load();
        const auto hw = get_hardware_info();
        nlohmann::json j;
        j["status"]             = "ok";
        j["core_version"]       = core_version();
        j["uptime_ms"]          = uptime;
        j["launch_to_ready_ms"] = (ltr >= 0) ? nlohmann::json(ltr) : nlohmann::json(nullptr);
        j["hardware"]           = {
            {"cpu_brand", hw.cpu_brand},
            {"physical_cores", hw.physical_cores},
            {"logical_cores", hw.logical_cores},
            {"total_ram_bytes", hw.total_ram_bytes},
            {"os_version", hw.os_version},
            {"compiler_name", hw.compiler_name},
            {"compiler_version", hw.compiler_version},
            {"build_type", hw.build_type},
            {"optimization_flags", hw.optimization_flags},
            {"is_release_build", hw.is_release_build}
        };
        res.set_content(j.dump(), "application/json");
    });  // ← correctly closes the /api/health handler

    // Register FEM job routes and file routes BEFORE static handler.
    impl_->job_manager = std::make_unique<JobManager>(cfg_.max_jobs);
    register_fem_routes(svr, *impl_->job_manager, cfg_.results_dir,
                        [this]() { return launch_to_ready_ms_.load(); });
    register_file_routes(svr, impl_->file_store);
    register_device_routes(svr, impl_->device_registry);

    // Static file handler with MIME type lookup and path-traversal guard.
    svr.Get("/(.*)", [this](const httplib::Request& req, httplib::Response& res) {
        const std::string raw = req.matches[1].str();
        if (!path_is_safe(raw)) {
            res.status = 400; res.set_content("Bad path", "text/plain"); return;
        }
        const std::string rel = raw.empty() ? "index.html" : raw;
        namespace fs = std::filesystem;
        const fs::path full = fs::path(cfg_.ui_dir) / rel;
        std::ifstream ifs(full, std::ios::binary);
        if (!ifs) { res.status = 404; res.set_content("Not Found", "text/plain"); return; }
        res.set_content(
            std::string(std::istreambuf_iterator<char>(ifs), {}),
            mime_for(rel));
    });

    // Error handler: only rewrite 404/405; leave 400/401/403 from pre-routing intact.
    // (httplib calls this for any response with status >= 400.)
    svr.set_error_handler([](const httplib::Request&, httplib::Response& res) {
        add_security_headers(res);
        if      (res.status == 405) res.set_content("Method Not Allowed", "text/plain");
        else if (res.status == 404) res.set_content("Not Found", "text/plain");
        // 400/401/403 already have content from the pre-routing handler -- leave them.
    });

    // Bind to loopback only, never 0.0.0.0.
    int bound_port = 0;
    if (cfg_.preferred_port == 0) {
        bound_port = svr.bind_to_any_port("127.0.0.1");
        if (bound_port <= 0)
            throw std::runtime_error("Server: bind_to_any_port failed on 127.0.0.1");
    } else {
        if (!svr.bind_to_port("127.0.0.1", static_cast<int>(cfg_.preferred_port)))
            throw std::runtime_error("Server: bind failed on 127.0.0.1:" +
                std::to_string(cfg_.preferred_port));
        bound_port = static_cast<int>(cfg_.preferred_port);
    }
    port_ = static_cast<uint16_t>(bound_port);

    if (cfg_.dev_mode)
        std::fprintf(stderr, "\n*** WARNING: dev mode -- auth disabled ***\n\n");


    impl_->listener_thread = std::thread([this]() { impl_->svr.listen_after_bind(); });
    return port_;
}

void Server::stop() {
    if (impl_ && impl_->svr.is_running()) {
        impl_->svr.stop();
        if (impl_->listener_thread.joinable()) impl_->listener_thread.join();
    }
}

void Server::set_launch_to_ready_ms(int64_t ms) noexcept {
    launch_to_ready_ms_.store(ms);
}

}  // namespace workbench
