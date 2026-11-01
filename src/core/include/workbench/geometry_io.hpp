#pragma once
// geometry_io.hpp -- ASCII STL and nodal CSV parsers for workbench_core.
//
// All parsing and validation is in C++. Inputs are raw buffers (std::string_view).
// Locale-independent (numbers parsed manually, never via locale-sensitive streams).
// Outputs are flat float32/uint32 arrays suitable for binary transport to the UI.
//
// Size caps (named constants -- document in API.md and LIMITS.md):
//   kMaxFileBytes      64 MiB  raw upload size
//   kMaxStlTriangles   2,000,000 triangles
//   kMaxCsvNodes       1,000,000 nodes
//
// Binary transport layout (little-endian):
//   STL:  [n_tri:u32][v0x,v0y,v0z, v1x,v1y,v1z, v2x,v2y,v2z : f32 * 9 * n_tri]
//          followed by [nx,ny,nz : f32 * 3 * n_tri] face normals
//   CSV:  [n_nodes:u32][x,y,z : f32 * 3 * n_nodes][temp : f32 * n_nodes][id : u32 * n_nodes]
//
// See docs/API.md for the full documented layout.
#include "workbench/status.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace workbench {

// ── Caps ─────────────────────────────────────────────────────────────────────
inline constexpr std::size_t  kMaxFileBytes    = 64ULL * 1024 * 1024;  // 64 MiB
inline constexpr std::uint32_t kMaxStlTriangles = 2'000'000;
inline constexpr std::uint32_t kMaxCsvNodes     = 1'000'000;

// ── STL ──────────────────────────────────────────────────────────────────────
struct BoundingBox {
    float min_x, min_y, min_z;
    float max_x, max_y, max_z;
};

struct StlMesh {
    std::vector<float>    positions;    // 9 floats per triangle (x0y0z0 x1y1z1 x2y2z2)
    std::vector<float>    normals;      // 3 floats per triangle (recomputed face normal)
    BoundingBox           bounds{};
    std::uint32_t         triangle_count{0};
    std::uint32_t         degenerate_count{0};  // warning, not error
    std::string           warning;              // non-empty if degenerate_count > 0
    // Note: STL is unitless. The caller must document the assumed unit.
};

// Parse ASCII STL from a raw buffer.
// Returns Status::invalid() for: binary STL, malformed tokens (with line number),
// non-finite values, wrong vertex count, truncated file, zero triangles, empty file,
// file over kMaxFileBytes, file over kMaxStlTriangles.
[[nodiscard]] Result<StlMesh> parse_stl(std::string_view data);

// ── CSV ──────────────────────────────────────────────────────────────────────
struct PointCloud {
    std::vector<float>         xyz;          // 3 floats per node: x_m, y_m, z_m
    std::vector<float>         temperature_C; // 1 float per node
    std::vector<std::uint32_t> node_ids;      // 1 uint32 per node
    BoundingBox                bounds{};
    float                      min_temp_C{0};
    float                      max_temp_C{0};
    std::uint32_t              node_count{0};
    std::string                warning;       // e.g. temperature below -273.15 °C
};

// Parse nodal CSV. Header must be exactly: node_id,x_m,y_m,z_m,temperature_C
// Tolerates UTF-8 BOM and CRLF line endings.
// Returns Status::invalid() for: wrong header, non-finite values, duplicate node_id,
// text in numeric fields, extra/missing columns, empty file, file over caps.
[[nodiscard]] Result<PointCloud> parse_csv(std::string_view data);

// ── Binary serialisation ──────────────────────────────────────────────────────
// Serialise mesh/cloud to the documented little-endian binary layout.
[[nodiscard]] std::string serialise_stl_bin(const StlMesh& mesh);
[[nodiscard]] std::string serialise_csv_bin(const PointCloud& cloud);

}  // namespace workbench