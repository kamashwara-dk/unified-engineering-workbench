#pragma once
// server.hpp -- loopback HTTP server for the Workbench.
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

namespace workbench {

struct ServerConfig {
    std::string ui_dir;            // directory served as static files
    std::string token;             // 32 hex chars; empty means dev mode
    std::string results_dir;       // where export CSVs are written (relative)
    uint16_t    preferred_port{0}; // 0 = OS picks a free port
    bool        dev_mode{false};   // disables token/cookie check
    int         max_jobs{2};       // max concurrent FEM jobs
};

class Server {
public:
    explicit Server(ServerConfig cfg);
    ~Server();

    // Bind and start. Returns actual port. Throws on bind failure.
    [[nodiscard]] uint16_t start();

    // Graceful stop.
    void stop();

    // Thread-safe; call from any thread after start().
    void set_launch_to_ready_ms(int64_t ms) noexcept;

    [[nodiscard]] uint16_t          port()  const noexcept { return port_; }
    [[nodiscard]] const std::string& token() const noexcept { return cfg_.token; }

private:
    struct Impl;
    Impl*        impl_{nullptr};
    uint16_t     port_{0};
    std::atomic<int64_t> launch_to_ready_ms_{-1};
    ServerConfig cfg_;
};

}  // namespace workbench