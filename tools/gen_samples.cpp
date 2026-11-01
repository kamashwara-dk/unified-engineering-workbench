// gen_samples.cpp -- generates sample data files for testing the importer.
// Output goes to data/ (relative paths only).
// Normal mode: small synthetic L-bracket STL + nodal CSV.
// --stress N: generates files of approximately N MiB for limit testing.
//
// All data is synthetic and computer-generated. See data/README.md for provenance.
#include "workbench/geometry_io.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// ── Atomic write helper ───────────────────────────────────────────────────────
static bool write_file(const std::string& path, const std::string& content) {
    const std::string tmp = path + ".tmp";
    {
        std::ofstream ofs(tmp, std::ios::binary | std::ios::trunc);
        if (!ofs) { std::fprintf(stderr, "ERROR: cannot write %s\n", tmp.c_str()); return false; }
        ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    if (ec) { std::fprintf(stderr, "ERROR rename: %s\n", ec.message().c_str()); return false; }
    return true;
}

// ── STL generation ────────────────────────────────────────────────────────────
// Generates a simple L-bracket shape as ASCII STL (two rectangular prisms joined).
// Returns the STL text.
static std::string make_stl_lbracket() {
    // Each face of the bracket is 2 triangles. L-bracket = 2 boxes joined.
    // Box A: x=[0,1], y=[0,3], z=[0,0.5] (horizontal arm)
    // Box B: x=[0,1], y=[0,0.5], z=[0,3] (vertical arm, shares y=[0,0.5])
    std::string out;
    out.reserve(4096);
    out += "solid synthetic_lbracket\n";

    // Helper: emit one quad (4 corners) as 2 triangles.
    auto quad = [&](float ax, float ay, float az,
                    float bx, float by, float bz,
                    float cx, float cy, float cz,
                    float dx, float dy, float dz) {
        // Triangle 1: A, B, C
        out += "  facet normal 0 0 0\n    outer loop\n";
        out += "      vertex " + std::to_string(ax) + " " + std::to_string(ay) + " " + std::to_string(az) + "\n";
        out += "      vertex " + std::to_string(bx) + " " + std::to_string(by) + " " + std::to_string(bz) + "\n";
        out += "      vertex " + std::to_string(cx) + " " + std::to_string(cy) + " " + std::to_string(cz) + "\n";
        out += "    endloop\n  endfacet\n";
        // Triangle 2: A, C, D
        out += "  facet normal 0 0 0\n    outer loop\n";
        out += "      vertex " + std::to_string(ax) + " " + std::to_string(ay) + " " + std::to_string(az) + "\n";
        out += "      vertex " + std::to_string(cx) + " " + std::to_string(cy) + " " + std::to_string(cz) + "\n";
        out += "      vertex " + std::to_string(dx) + " " + std::to_string(dy) + " " + std::to_string(dz) + "\n";
        out += "    endloop\n  endfacet\n";
    };

    // Box A faces (horizontal arm): x=[0,1], y=[0,3], z=[0,0.5]
    quad(0,0,0, 1,0,0, 1,3,0, 0,3,0);  // bottom -Z
    quad(0,3,0.5f, 1,3,0.5f, 1,0,0.5f, 0,0,0.5f);  // top +Z
    quad(0,0,0, 0,3,0, 0,3,0.5f, 0,0,0.5f);  // left -X
    quad(1,0,0, 1,0,0.5f, 1,3,0.5f, 1,3,0);  // right +X
    quad(0,0,0, 0,0,0.5f, 1,0,0.5f, 1,0,0);  // front -Y
    quad(0,3,0, 1,3,0, 1,3,0.5f, 0,3,0.5f);  // back +Y

    // Box B faces (vertical arm): x=[0,1], y=[0,0.5], z=[0,3]
    // (shares bottom face with box A so skip overlapping -Z at z=0 for y=[0,0.5])
    quad(0,0,0, 0,0,3, 0,0.5f,3, 0,0.5f,0);  // left -X
    quad(1,0,0, 1,0.5f,0, 1,0.5f,3, 1,0,3);  // right +X
    quad(0,0,3, 1,0,3, 1,0.5f,3, 0,0.5f,3);  // top +Z
    quad(0,0.5f,0, 0,0.5f,3, 1,0.5f,3, 1,0.5f,0);  // back +Y

    out += "endsolid synthetic_lbracket\n";
    return out;
}

// ── CSV generation ────────────────────────────────────────────────────────────
// Generates a synthetic temperature field over a grid of nodes.
// T(x,y,z) = 100 * exp(-((x-0.5)^2 + (y-1.5)^2 + z^2))  [smooth Gaussian blob]
static std::string make_csv(int nx, int ny, int nz) {
    std::string out;
    out.reserve(nx * ny * nz * 50);
    out = "node_id,x_m,y_m,z_m,temperature_C\n";
    int nid = 0;
    for (int iz = 0; iz < nz; ++iz)
    for (int iy = 0; iy < ny; ++iy)
    for (int ix = 0; ix < nx; ++ix) {
        const float x = static_cast<float>(ix) / (nx > 1 ? nx - 1 : 1);
        const float y = 3.0f * static_cast<float>(iy) / (ny > 1 ? ny - 1 : 1);
        const float z = 0.5f * static_cast<float>(iz) / (nz > 1 ? nz - 1 : 1);
        const float dx = x - 0.5f, dy = y - 1.5f, dz = z;
        const float T  = 100.0f * std::exp(-(dx*dx + dy*dy + dz*dz));
        out += std::to_string(nid) + ","
             + std::to_string(x)   + ","
             + std::to_string(y)   + ","
             + std::to_string(z)   + ","
             + std::to_string(T)   + "\n";
        ++nid;
    }
    return out;
}

int main(int argc, char* argv[]) {
    std::string out_dir = "data";
    bool stress = false;
    int stress_mib = 10;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--stress") == 0 && i+1 < argc)
            { stress = true; stress_mib = std::atoi(argv[++i]); }
        else if (std::strcmp(argv[i], "--out") == 0 && i+1 < argc)
            out_dir = argv[++i];
    }

    std::error_code ec;
    std::filesystem::create_directories(out_dir, ec);

    if (stress) {
        // Stress STL: triangles until ~stress_mib MiB.
        // Each triangle is ~160 bytes ASCII. stress_mib*1024*1024/160 triangles.
        const int n_tris = (stress_mib * 1024 * 1024) / 160;
        std::string stl = "solid stress_stl\n";
        for (int i = 0; i < n_tris; ++i) {
            const float f = static_cast<float>(i) * 0.001f;
            stl += "  facet normal 0 0 1\n    outer loop\n";
            stl += "      vertex " + std::to_string(f)       + " 0 0\n";
            stl += "      vertex " + std::to_string(f+0.5f)  + " 1 0\n";
            stl += "      vertex " + std::to_string(f+1.0f)  + " 0 0\n";
            stl += "    endloop\n  endfacet\n";
        }
        stl += "endsolid stress_stl\n";
        const std::string path = out_dir + "/stress_" + std::to_string(stress_mib) + "mib.stl";
        if (!write_file(path, stl)) return 1;
        std::fprintf(stdout, "Stress STL: %s  (%zu bytes, %d triangles)\n",
                     path.c_str(), stl.size(), n_tris);

        // Stress CSV: nodes until ~stress_mib MiB.
        // Each row is ~50 bytes. stress_mib*1024*1024/50 nodes.
        const int n_nodes = (stress_mib * 1024 * 1024) / 50;
        const int ny = static_cast<int>(std::cbrt(static_cast<double>(n_nodes))) + 1;
        std::string csv = make_csv(ny, ny, ny);
        const std::string cpath = out_dir + "/stress_" + std::to_string(stress_mib) + "mib.csv";
        if (!write_file(cpath, csv)) return 1;
        std::fprintf(stdout, "Stress CSV: %s  (%zu bytes, %d nodes)\n",
                     cpath.c_str(), csv.size(), ny*ny*ny);
        return 0;
    }

    // Normal mode: small L-bracket STL + Gaussian CSV.
    const std::string stl  = make_stl_lbracket();
    const std::string spath = out_dir + "/synthetic_lbracket.stl";
    if (!write_file(spath, stl)) return 1;
    std::fprintf(stdout, "STL: %s  (%zu bytes)\n", spath.c_str(), stl.size());

    // Validate it parses cleanly.
    auto stl_res = workbench::parse_stl(stl);
    if (!stl_res.ok()) {
        std::fprintf(stderr, "VALIDATION FAILED: %s\n", stl_res.error().message.c_str());
        return 1;
    }
    std::fprintf(stdout, "  Parsed OK: %u triangles, %u degenerate\n",
                 stl_res.value().triangle_count, stl_res.value().degenerate_count);

    const std::string csv  = make_csv(20, 30, 5); // 3000 nodes
    const std::string cpath = out_dir + "/synthetic_temperature.csv";
    if (!write_file(cpath, csv)) return 1;
    std::fprintf(stdout, "CSV: %s  (%zu bytes)\n", cpath.c_str(), csv.size());

    auto csv_res = workbench::parse_csv(csv);
    if (!csv_res.ok()) {
        std::fprintf(stderr, "VALIDATION FAILED: %s\n", csv_res.error().message.c_str());
        return 1;
    }
    std::fprintf(stdout, "  Parsed OK: %u nodes, T=[%.2f, %.2f] C\n",
                 csv_res.value().node_count,
                 csv_res.value().min_temp_C,
                 csv_res.value().max_temp_C);
    return 0;
}