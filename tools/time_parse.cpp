// time_parse.cpp -- measures parse time for STL and CSV files
#include "workbench/geometry_io.hpp"
#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

static std::string read_file(const char* path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { std::fprintf(stderr, "Cannot open: %s\n", path); return {}; }
    std::ostringstream ss; ss << f.rdbuf(); return ss.str();
}

static void time_parse(const char* path, bool is_stl) {
    const std::string data = read_file(path);
    if (data.empty()) return;
    const auto start = std::chrono::steady_clock::now();
    if (is_stl) {
        auto r = workbench::parse_stl(data);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (r.ok())
            std::fprintf(stdout, "STL %s: %.1f MiB, %u triangles, %lld ms\n",
                path, data.size()/1048576.0, r.value().triangle_count, (long long)ms);
        else
            std::fprintf(stdout, "STL %s: ERROR: %s\n", path, r.error().message.c_str());
    } else {
        auto r = workbench::parse_csv(data);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (r.ok())
            std::fprintf(stdout, "CSV %s: %.1f MiB, %u nodes, %lld ms\n",
                path, data.size()/1048576.0, r.value().node_count, (long long)ms);
        else
            std::fprintf(stdout, "CSV %s: ERROR: %s\n", path, r.error().message.c_str());
    }
}

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        const std::string p = argv[i];
        const bool is_stl = p.size() >= 4 && p.substr(p.size()-4) == ".stl";
        time_parse(argv[i], is_stl);
    }
    return 0;
}