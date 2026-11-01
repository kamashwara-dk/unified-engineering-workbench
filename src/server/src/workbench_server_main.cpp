// workbench_server_main.cpp -- cross-platform console server for dev + tests.
// Usage: workbench_server [--port N] [--ui-dir PATH] [--dev]
#include "workbench/server.hpp"
#include "workbench/token.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>

int main(int argc, char* argv[]) {
    workbench::ServerConfig cfg;
    cfg.ui_dir = "src/ui/dist";
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--port" && i+1 < argc) { cfg.preferred_port = static_cast<uint16_t>(std::stoi(argv[++i])); }
        else if (arg == "--ui-dir" && i+1 < argc) { cfg.ui_dir = argv[++i]; }
        else if (arg == "--dev") { cfg.dev_mode = true; }
    }
    if (!cfg.dev_mode) { cfg.token = workbench::generate_token(); }
    workbench::Server server(std::move(cfg));
    const uint16_t port = server.start();
    const std::string url = "http://127.0.0.1:" + std::to_string(port)
        + (cfg.token.empty() ? "" : "/?t=" + cfg.token);
    fprintf(stdout, "Workbench server listening on %s\n", url.c_str());
    fprintf(stdout, "Press Enter to stop.\n");
    (void)getchar();
    server.stop();
    return 0;
}
