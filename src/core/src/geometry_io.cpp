// geometry_io.cpp -- ASCII STL and nodal CSV parsers.
// All number parsing is locale-independent (manual parsing with std::strtof/strtod).
// Line numbers are tracked from 1 for error messages.
#include "workbench/geometry_io.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace workbench {
namespace {

// ── Number helpers ────────────────────────────────────────────────────────────

// Parse float from string_view, locale-independent.
// Returns false on failure (non-finite result or bad syntax).
bool parse_float(std::string_view sv, float& out) noexcept {
    if (sv.empty()) return false;
    // Need null-terminated string; copy to a small buffer.
    char buf[64];
    if (sv.size() >= sizeof(buf)) return false;
    std::memcpy(buf, sv.data(), sv.size());
    buf[sv.size()] = '\0';
    char* end = nullptr;
    const float v = std::strtof(buf, &end);
    if (end != buf + sv.size()) return false;
    if (!std::isfinite(v)) return false;
    out = v;
    return true;
}

// Parse uint32 from string_view. Returns false on failure or overflow.
bool parse_uint32(std::string_view sv, std::uint32_t& out) noexcept {
    if (sv.empty()) return false;
    std::uint64_t acc = 0;
    for (const char c : sv) {
        if (c < '0' || c > '9') return false;
        acc = acc * 10 + static_cast<unsigned>(c - '0');
        if (acc > std::numeric_limits<std::uint32_t>::max()) return false;
    }
    out = static_cast<std::uint32_t>(acc);
    return true;
}

// ── Case-insensitive compare ──────────────────────────────────────────────────
bool iequal(std::string_view a, const char* b) noexcept {
    std::size_t n = std::strlen(b);
    if (a.size() != n) return false;
    for (std::size_t i = 0; i < n; ++i)
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    return true;
}

// ── Tokeniser ─────────────────────────────────────────────────────────────────
// Splits on whitespace, tracks line numbers (1-based).
struct Tokeniser {
    std::string_view src;
    std::size_t      pos{0};
    int              line{1};

    // Returns the next whitespace-delimited token, or empty if EOF.
    std::string_view next() noexcept {
        // Skip whitespace and count newlines.
        while (pos < src.size() && std::isspace(static_cast<unsigned char>(src[pos]))) {
            if (src[pos] == '\n') ++line;
            ++pos;
        }
        if (pos == src.size()) return {};
        const std::size_t start = pos;
        while (pos < src.size() && !std::isspace(static_cast<unsigned char>(src[pos])))
            ++pos;
        return src.substr(start, pos - start);
    }

    // Peek without consuming.
    std::string_view peek() noexcept {
        const std::size_t save_pos  = pos;
        const int         save_line = line;
        auto tok = next();
        pos  = save_pos;
        line = save_line;
        return tok;
    }

    bool eof() const noexcept { return pos >= src.size(); }
};

// ── Binary STL detection ──────────────────────────────────────────────────────
// Binary STL: 80-byte header + uint32 triangle count + 50 bytes per triangle.
// Some binary files start with "solid" in the header -- check file size too.
bool is_binary_stl(std::string_view data) noexcept {
    if (data.size() < 84) return false;
    std::uint32_t tri_count = 0;
    std::memcpy(&tri_count, data.data() + 80, 4);
    // Little-endian on x86; works on all modern platforms.
    const std::uint64_t expected_size = 84ULL + 50ULL * tri_count;
    return data.size() == expected_size && tri_count > 0;
}

// ── Normal recomputation ──────────────────────────────────────────────────────
struct Vec3 { float x, y, z; };
static Vec3 cross(Vec3 a, Vec3 b) noexcept {
    return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
}
static Vec3 sub(Vec3 a, Vec3 b) noexcept { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
static float len(Vec3 v) noexcept { return std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z); }
static Vec3 normalise(Vec3 v) noexcept {
    float l = len(v);
    if (l < 1e-30f) return {0,0,1};
    return {v.x/l, v.y/l, v.z/l};
}

// ── Bounding box update ───────────────────────────────────────────────────────
static void bbox_init(BoundingBox& bb) noexcept {
    bb = { std::numeric_limits<float>::max(),  std::numeric_limits<float>::max(),
           std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(),
          -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() };
}
static void bbox_update(BoundingBox& bb, float x, float y, float z) noexcept {
    if (x < bb.min_x) bb.min_x = x;  if (x > bb.max_x) bb.max_x = x;
    if (y < bb.min_y) bb.min_y = y;  if (y > bb.max_y) bb.max_y = y;
    if (z < bb.min_z) bb.min_z = z;  if (z > bb.max_z) bb.max_z = z;
}

}  // namespace
// ── ASCII STL parser ──────────────────────────────────────────────────────────
Result<StlMesh> parse_stl(std::string_view data) {
    if (data.empty())
        return Status::invalid("line 1: empty file");
    if (data.size() > kMaxFileBytes)
        return Status::invalid("file exceeds the 64 MiB size cap");

    // Binary STL check: must happen before tokenising because some binary
    // files have "solid" in their 80-byte header.
    if (is_binary_stl(data))
        return Status::invalid("binary STL is not supported; export as ASCII STL from your CAD tool");

    Tokeniser tok{data};

    // Expect "solid" keyword (case-insensitive).
    auto kw = tok.next();
    if (kw.empty())
        return Status::invalid("line 1: empty file");
    if (!iequal(kw, "solid"))
        return Status::invalid("line " + std::to_string(tok.line)
                               + ": expected 'solid', got '" + std::string(kw) + "'");

    // Skip optional solid name (rest of the "solid" line).
    // Skip optional solid name: consume tokens until the line number advances
    // (which happens when the next() call crosses a newline) or until EOF.
    {
        const int solid_line = tok.line;
        while (tok.line == solid_line && !tok.eof()) {
            // peek to check if the next token is on the same line
            const std::size_t save_pos  = tok.pos;
            const int         save_line = tok.line;
            const auto t = tok.next();
            // If we crossed to a new line, put it back and stop.
            if (tok.line > solid_line) { tok.pos = save_pos; tok.line = save_line; break; }
            if (t.empty()) break;
            // Discard this name token (it was consumed above).
        }
    }

    StlMesh mesh;
    bbox_init(mesh.bounds);

    while (true) {
        auto word = tok.next();
        if (word.empty())
            return Status::invalid("line " + std::to_string(tok.line) + ": truncated file, expected 'facet' or 'endsolid'");
        if (iequal(word, "endsolid")) break;
        if (!iequal(word, "facet"))
            return Status::invalid("line " + std::to_string(tok.line)
                                   + ": expected 'facet', got '" + std::string(word) + "'");

        // "normal nx ny nz"
        auto nw = tok.next();
        if (!iequal(nw, "normal"))
            return Status::invalid("line " + std::to_string(tok.line)
                                   + ": expected 'normal' after 'facet', got '" + std::string(nw) + "'");
        float nx_in, ny_in, nz_in;
        for (float* fp : {&nx_in, &ny_in, &nz_in}) {
            const auto sv = tok.next();
            if (!parse_float(sv, *fp))
                return Status::invalid("line " + std::to_string(tok.line)
                                       + ": invalid float in normal: '" + std::string(sv) + "'");
        }

        // "outer loop"
        auto ow = tok.next();
        if (!iequal(ow, "outer"))
            return Status::invalid("line " + std::to_string(tok.line)
                                   + ": expected 'outer loop', got '" + std::string(ow) + "'");
        auto lw = tok.next();
        if (!iequal(lw, "loop"))
            return Status::invalid("line " + std::to_string(tok.line)
                                   + ": expected 'loop' after 'outer', got '" + std::string(lw) + "'");

        // 3 x "vertex x y z"
        std::array<float, 9> verts{};
        for (int v = 0; v < 3; ++v) {
            auto vw = tok.next();
            if (!iequal(vw, "vertex"))
                return Status::invalid("line " + std::to_string(tok.line)
                                       + ": expected 'vertex', got '" + std::string(vw)
                                       + "' (vertex " + std::to_string(v+1) + " of 3)");
            for (int c = 0; c < 3; ++c) {
                const auto sv = tok.next();
                if (!parse_float(sv, verts[v*3+c]))
                    return Status::invalid("line " + std::to_string(tok.line)
                                           + ": invalid float in vertex: '" + std::string(sv) + "'");
                bbox_update(mesh.bounds, verts[v*3], verts[v*3+1], verts[v*3+2]);
            }
        }

        // "endloop"
        auto elw = tok.next();
        if (!iequal(elw, "endloop"))
            return Status::invalid("line " + std::to_string(tok.line)
                                   + ": expected 'endloop', got '" + std::string(elw) + "'");

        // "endfacet"
        auto efw = tok.next();
        if (!iequal(efw, "endfacet"))
            return Status::invalid("line " + std::to_string(tok.line)
                                   + ": expected 'endfacet', got '" + std::string(efw) + "'");

        // Bounding box update for all 3 vertices.
        for (int v = 0; v < 3; ++v)
            bbox_update(mesh.bounds, verts[v*3], verts[v*3+1], verts[v*3+2]);

        // Recompute face normal (ignore the stored one).
        Vec3 p0{verts[0],verts[1],verts[2]};
        Vec3 p1{verts[3],verts[4],verts[5]};
        Vec3 p2{verts[6],verts[7],verts[8]};
        Vec3 edge1 = sub(p1, p0);
        Vec3 edge2 = sub(p2, p0);
        Vec3 n     = normalise(cross(edge1, edge2));
        if (len(cross(edge1, edge2)) < 1e-30f) {
            ++mesh.degenerate_count;
        }

        for (float val : verts)
            mesh.positions.push_back(val);
        mesh.normals.push_back(n.x);
        mesh.normals.push_back(n.y);
        mesh.normals.push_back(n.z);
        ++mesh.triangle_count;

        if (mesh.triangle_count > kMaxStlTriangles)
            return Status::invalid("triangle count exceeds the 2,000,000 cap");
    }

    if (mesh.triangle_count == 0)
        return Status::invalid("file contains no triangles; check the STL was exported correctly");

    if (mesh.degenerate_count > 0) {
        mesh.warning = "WARNING: " + std::to_string(mesh.degenerate_count)
                       + " degenerate triangle(s) found (area ~0); normals set to (0,0,1)."
                         " Check the mesh for zero-area faces.";
    }
    return mesh;
}
// ── CSV line splitter ─────────────────────────────────────────────────────────
// Splits a line on commas, respecting nothing fancy (no quoting).
static std::vector<std::string_view> split_csv_line(std::string_view line) {
    std::vector<std::string_view> fields;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= line.size(); ++i) {
        if (i == line.size() || line[i] == ',') {
            fields.push_back(line.substr(start, i - start));
            start = i + 1;
        }
    }
    return fields;
}

// Trim whitespace (but NOT commas) from both ends.
static std::string_view trim(std::string_view sv) noexcept {
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front()))) sv.remove_prefix(1);
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back())))  sv.remove_suffix(1);
    return sv;
}

// ── Nodal CSV parser ──────────────────────────────────────────────────────────
Result<PointCloud> parse_csv(std::string_view data) {
    if (data.empty())
        return Status::invalid("line 1: empty file");
    if (data.size() > kMaxFileBytes)
        return Status::invalid("file exceeds the 64 MiB size cap");

    // Strip UTF-8 BOM (EF BB BF).
    if (data.size() >= 3 &&
        static_cast<unsigned char>(data[0]) == 0xEF &&
        static_cast<unsigned char>(data[1]) == 0xBB &&
        static_cast<unsigned char>(data[2]) == 0xBF)
        data.remove_prefix(3);

    // Split into lines.
    std::vector<std::string_view> lines;
    {
        std::size_t pos = 0;
        while (pos <= data.size()) {
            std::size_t nl = data.find('\n', pos);
            if (nl == std::string_view::npos) nl = data.size();
            std::string_view line = data.substr(pos, nl - pos);
            // Strip trailing CR.
            if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
            lines.push_back(line);
            pos = nl + 1;
        }
    }

    if (lines.empty() || (lines.size() == 1 && trim(lines[0]).empty()))
        return Status::invalid("line 1: empty file");

    // Validate header.
    const std::string_view expected_header = "node_id,x_m,y_m,z_m,temperature_C";
    if (trim(lines[0]) != expected_header) {
        return Status::invalid(
            "line 1: wrong CSV header.\n"
            "  Expected: " + std::string(expected_header) + "\n"
            "  Found:    " + std::string(trim(lines[0])));
    }

    PointCloud cloud;
    BoundingBox bb;
    bbox_init(bb);
    float min_t =  std::numeric_limits<float>::max();
    float max_t = -std::numeric_limits<float>::max();
    bool has_cold_warning = false;

    std::unordered_set<std::uint32_t> seen_ids;

    for (std::size_t li = 1; li < lines.size(); ++li) {
        const int lineno = static_cast<int>(li + 1);
        const std::string_view raw = lines[li];
        if (trim(raw).empty()) continue; // blank trailing lines OK

        const auto fields = split_csv_line(raw);
        if (fields.size() != 5)
            return Status::invalid(
                "line " + std::to_string(lineno) +
                ": expected 5 fields, got " + std::to_string(fields.size()));

        // node_id
        std::uint32_t nid = 0;
        const auto nid_sv = trim(fields[0]);
        if (!parse_uint32(nid_sv, nid))
            return Status::invalid(
                "line " + std::to_string(lineno) +
                ": node_id must be a non-negative integer, got '" + std::string(nid_sv) + "'");
        if (!seen_ids.insert(nid).second)
            return Status::invalid(
                "line " + std::to_string(lineno) +
                ": duplicate node_id " + std::to_string(nid));

        // x_m, y_m, z_m, temperature_C
        float x, y, z, t;
        const char* field_names[] = {"x_m", "y_m", "z_m", "temperature_C"};
        float* vals[] = {&x, &y, &z, &t};
        for (int fi = 0; fi < 4; ++fi) {
            const auto sv = trim(fields[fi + 1]);
            if (!parse_float(sv, *vals[fi]))
                return Status::invalid(
                    "line " + std::to_string(lineno) +
                    ": invalid value for " + field_names[fi] +
                    " (must be a finite number): '" + std::string(sv) + "'");
        }

        if (t < -273.15f) has_cold_warning = true;

        cloud.xyz.push_back(x);
        cloud.xyz.push_back(y);
        cloud.xyz.push_back(z);
        cloud.temperature_C.push_back(t);
        cloud.node_ids.push_back(nid);
        bbox_update(bb, x, y, z);
        if (t < min_t) min_t = t;
        if (t > max_t) max_t = t;
        ++cloud.node_count;

        if (cloud.node_count > kMaxCsvNodes)
            return Status::invalid("node count exceeds the 1,000,000 cap");
    }

    if (cloud.node_count == 0)
        return Status::invalid("line 2: header-only file; no data rows found");

    cloud.bounds   = bb;
    cloud.min_temp_C = min_t;
    cloud.max_temp_C = max_t;
    if (has_cold_warning)
        cloud.warning = "WARNING: one or more temperature values are below absolute zero (-273.15 deg C). Check units.";

    return cloud;
}

// ── Binary serialisers (little-endian) ───────────────────────────────────────
// Layout documented in docs/API.md.
// STL: [n_tri:u32][pos: f32*9*n][normals: f32*3*n]
// CSV: [n_nodes:u32][xyz: f32*3*n][temp: f32*n][ids: u32*n]

static void push_u32(std::string& out, std::uint32_t v) {
    out.append(reinterpret_cast<const char*>(&v), 4);
}
static void push_f32(std::string& out, float v) {
    out.append(reinterpret_cast<const char*>(&v), 4);
}

std::string serialise_stl_bin(const StlMesh& mesh) {
    std::string out;
    out.reserve(4 + mesh.positions.size()*4 + mesh.normals.size()*4);
    push_u32(out, mesh.triangle_count);
    for (float f : mesh.positions) push_f32(out, f);
    for (float f : mesh.normals)   push_f32(out, f);
    return out;
}

std::string serialise_csv_bin(const PointCloud& cloud) {
    std::string out;
    out.reserve(4 + cloud.xyz.size()*4 + cloud.temperature_C.size()*4 + cloud.node_ids.size()*4);
    push_u32(out, cloud.node_count);
    for (float f : cloud.xyz)          push_f32(out, f);
    for (float f : cloud.temperature_C) push_f32(out, f);
    for (std::uint32_t id : cloud.node_ids) push_u32(out, id);
    return out;
}

}  // namespace workbench
