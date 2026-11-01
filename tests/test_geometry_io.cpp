// test_geometry_io.cpp -- Phase 3 importer tests.
// Tests valid files, malformed corpus, boundary sizes, round-trip counts.
#include "catch_amalgamated.hpp"
#include "workbench/geometry_io.hpp"
#include <cmath>
#include <limits>
#include <string>

using namespace workbench;

// ── STL helpers ──────────────────────────────────────────────────────────────
static std::string one_triangle_stl(float x0=0,float y0=0,float z0=0,
                                    float x1=1,float y1=0,float z1=0,
                                    float x2=0,float y2=1,float z2=0) {
    return "solid test\n"
           "  facet normal 0 0 1\n    outer loop\n"
           "      vertex " + std::to_string(x0) + " " + std::to_string(y0) + " " + std::to_string(z0) + "\n"
           "      vertex " + std::to_string(x1) + " " + std::to_string(y1) + " " + std::to_string(z1) + "\n"
           "      vertex " + std::to_string(x2) + " " + std::to_string(y2) + " " + std::to_string(z2) + "\n"
           "    endloop\n  endfacet\n"
           "endsolid test\n";
}

static std::string csv_header() { return "node_id,x_m,y_m,z_m,temperature_C\n"; }
static std::string csv_row(int id, float x, float y, float z, float t) {
    return std::to_string(id) + "," + std::to_string(x) + "," + std::to_string(y) + ","
           + std::to_string(z) + "," + std::to_string(t) + "\n";
}

// ═══════════════════════════════════════════════════════════════════════════════
// STL VALID
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("STL: valid single triangle", "[stl][valid]") {
    auto r = parse_stl(one_triangle_stl());
    REQUIRE(r.ok());
    CHECK(r.value().triangle_count == 1);
    CHECK(r.value().positions.size() == 9);
    CHECK(r.value().normals.size()   == 3);
    CHECK(r.value().degenerate_count == 0);
    CHECK(r.value().warning.empty());
}

TEST_CASE("STL: case-insensitive keywords", "[stl][valid]") {
    const std::string stl =
        "SOLID test\n"
        "  FACET NORMAL 0 0 1\n    OUTER LOOP\n"
        "      VERTEX 0 0 0\n      VERTEX 1 0 0\n      VERTEX 0 1 0\n"
        "    ENDLOOP\n  ENDFACET\n"
        "ENDSOLID test\n";
    REQUIRE(parse_stl(stl).ok());
}

TEST_CASE("STL: recomputed normal differs from stored", "[stl][valid]") {
    // Triangle in XY plane; stored normal is wrong -- parser should recompute.
    const std::string stl =
        "solid test\n"
        "  facet normal 1 0 0\n    outer loop\n"  // wrong normal stored
        "      vertex 0 0 0\n      vertex 1 0 0\n      vertex 0 1 0\n"
        "    endloop\n  endfacet\nendsolid test\n";
    auto r = parse_stl(stl);
    REQUIRE(r.ok());
    // Recomputed normal for XY-plane triangle should be (0,0,1) or (0,0,-1).
    const float nz = r.value().normals[2];
    CHECK(std::abs(std::abs(nz) - 1.0f) < 1e-5f);
}

TEST_CASE("STL: bounding box correct", "[stl][valid]") {
    auto r = parse_stl(one_triangle_stl(0,0,0, 2,0,0, 0,3,0));
    REQUIRE(r.ok());
    const auto& bb = r.value().bounds;
    CHECK(bb.min_x == 0.0f);  CHECK(bb.max_x == 2.0f);
    CHECK(bb.min_y == 0.0f);  CHECK(bb.max_y == 3.0f);
    CHECK(bb.min_z == 0.0f);  CHECK(bb.max_z == 0.0f);
}

TEST_CASE("STL: degenerate triangle is warning not error", "[stl][valid]") {
    // Three vertices on same point -> degenerate.
    auto r = parse_stl(one_triangle_stl(0,0,0, 0,0,0, 0,0,0));
    REQUIRE(r.ok());
    CHECK(r.value().degenerate_count == 1);
    CHECK(!r.value().warning.empty());
}

TEST_CASE("STL: solid with optional name parses", "[stl][valid]") {
    const std::string stl =
        "solid my bracket v1.2\n"
        "  facet normal 0 0 1\n    outer loop\n"
        "      vertex 0 0 0\n      vertex 1 0 0\n      vertex 0 1 0\n"
        "    endloop\n  endfacet\nendsolid my bracket v1.2\n";
    REQUIRE(parse_stl(stl).ok());
}

// ═══════════════════════════════════════════════════════════════════════════════
// STL MALFORMED
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("STL: empty file returns error with message", "[stl][error]") {
    auto r = parse_stl("");
    REQUIRE(!r.ok());
    CHECK(!r.error().message.empty());
    INFO(r.error().message);
}

TEST_CASE("STL: binary STL rejected with actionable message", "[stl][error]") {
    // Build a minimal valid binary STL (1 triangle).
    std::string bin(84 + 50, '\0');
    bin[80] = 1; // triangle count = 1
    auto r = parse_stl(bin);
    REQUIRE(!r.ok());
    INFO(r.error().message);
    CHECK(r.error().message.find("binary STL") != std::string::npos);
    CHECK(r.error().message.find("ASCII") != std::string::npos);
}

TEST_CASE("STL: binary starting with 'solid' still detected", "[stl][error]") {
    // 80-byte header starting with "solid" + valid count + payload.
    std::string bin(84 + 50, '\0');
    std::memcpy(bin.data(), "solid fake binary", 17);
    bin[80] = 1;
    auto r = parse_stl(bin);
    REQUIRE(!r.ok());
    CHECK(r.error().message.find("binary") != std::string::npos);
}

TEST_CASE("STL: zero triangles returns error", "[stl][error]") {
    const std::string stl = "solid test\nendsolid test\n";
    auto r = parse_stl(stl);
    REQUIRE(!r.ok());
    CHECK(!r.error().message.empty());
    CHECK(r.error().message.find("no triangles") != std::string::npos);
}

TEST_CASE("STL: missing endloop returns error with line number", "[stl][error]") {
    const std::string stl =
        "solid test\n"
        "  facet normal 0 0 1\n    outer loop\n"
        "      vertex 0 0 0\n      vertex 1 0 0\n      vertex 0 1 0\n"
        "    endfacet\nendsolid test\n";  // missing endloop
    auto r = parse_stl(stl);
    REQUIRE(!r.ok());
    INFO(r.error().message);
    // Message should contain a line number.
    CHECK(r.error().message.find("line ") != std::string::npos);
}

TEST_CASE("STL: only 2 vertices returns error with line number", "[stl][error]") {
    const std::string stl =
        "solid test\n"
        "  facet normal 0 0 1\n    outer loop\n"
        "      vertex 0 0 0\n      vertex 1 0 0\n"  // only 2 vertices
        "    endloop\n  endfacet\nendsolid test\n";
    auto r = parse_stl(stl);
    REQUIRE(!r.ok());
    CHECK(r.error().message.find("line ") != std::string::npos);
}

TEST_CASE("STL: NaN vertex value returns error", "[stl][error]") {
    const std::string stl =
        "solid test\n"
        "  facet normal 0 0 1\n    outer loop\n"
        "      vertex nan 0 0\n      vertex 1 0 0\n      vertex 0 1 0\n"
        "    endloop\n  endfacet\nendsolid test\n";
    auto r = parse_stl(stl);
    REQUIRE(!r.ok());
    CHECK(!r.error().message.empty());
}

TEST_CASE("STL: Inf vertex value returns error", "[stl][error]") {
    const std::string stl =
        "solid test\n"
        "  facet normal 0 0 1\n    outer loop\n"
        "      vertex inf 0 0\n      vertex 1 0 0\n      vertex 0 1 0\n"
        "    endloop\n  endfacet\nendsolid test\n";
    auto r = parse_stl(stl);
    REQUIRE(!r.ok());
}

TEST_CASE("STL: 1e999 (overflow to inf) returns error", "[stl][error]") {
    const std::string stl =
        "solid test\n"
        "  facet normal 0 0 1\n    outer loop\n"
        "      vertex 1e999 0 0\n      vertex 1 0 0\n      vertex 0 1 0\n"
        "    endloop\n  endfacet\nendsolid test\n";
    auto r = parse_stl(stl);
    REQUIRE(!r.ok());
}

TEST_CASE("STL: truncated file returns error", "[stl][error]") {
    const std::string stl =
        "solid test\n"
        "  facet normal 0 0 1\n    outer loop\n"
        "      vertex 0 0 0\n      vertex 1 0 0\n";  // truncated
    auto r = parse_stl(stl);
    REQUIRE(!r.ok());
    CHECK(!r.error().message.empty());
}

TEST_CASE("STL: file over size cap returns error", "[stl][error]") {
    // Just over 64 MiB worth of data.
    const std::string big(workbench::kMaxFileBytes + 1, 'x');
    auto r = parse_stl(big);
    REQUIRE(!r.ok());
    CHECK(r.error().message.find("64 MiB") != std::string::npos);
}

TEST_CASE("STL: round-trip triangle count matches", "[stl][valid]") {
    // 3 triangles.
    const std::string stl =
        "solid t\n"
        "  facet normal 0 0 1\n    outer loop\n"
        "      vertex 0 0 0\n      vertex 1 0 0\n      vertex 0 1 0\n"
        "    endloop\n  endfacet\n"
        "  facet normal 0 0 1\n    outer loop\n"
        "      vertex 2 0 0\n      vertex 3 0 0\n      vertex 2 1 0\n"
        "    endloop\n  endfacet\n"
        "  facet normal 0 0 1\n    outer loop\n"
        "      vertex 4 0 0\n      vertex 5 0 0\n      vertex 4 1 0\n"
        "    endloop\n  endfacet\n"
        "endsolid t\n";
    auto r = parse_stl(stl);
    REQUIRE(r.ok());
    CHECK(r.value().triangle_count == 3);
    CHECK(r.value().positions.size() == 27);  // 3 * 9
    CHECK(r.value().normals.size()   == 9);   // 3 * 3
}

// ═══════════════════════════════════════════════════════════════════════════════
// CSV VALID
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("CSV: valid single row", "[csv][valid]") {
    const std::string csv = csv_header() + csv_row(0, 1.0f, 2.0f, 3.0f, 100.0f);
    auto r = parse_csv(csv);
    REQUIRE(r.ok());
    CHECK(r.value().node_count == 1);
    CHECK(r.value().node_ids[0] == 0);
    CHECK(r.value().xyz[0] == 1.0f);
    CHECK(r.value().temperature_C[0] == 100.0f);
}

TEST_CASE("CSV: UTF-8 BOM tolerated", "[csv][valid]") {
    const std::string bom = "\xEF\xBB\xBF";
    const std::string csv = bom + csv_header() + csv_row(0, 0,0,0, 25.0f);
    auto r = parse_csv(csv);
    REQUIRE(r.ok());
    CHECK(r.value().node_count == 1);
}

TEST_CASE("CSV: CRLF line endings tolerated", "[csv][valid]") {
    const std::string csv = "node_id,x_m,y_m,z_m,temperature_C\r\n"
                            "0,1.0,2.0,3.0,100.0\r\n";
    auto r = parse_csv(csv);
    REQUIRE(r.ok());
    CHECK(r.value().node_count == 1);
}

TEST_CASE("CSV: blank trailing lines are OK", "[csv][valid]") {
    const std::string csv = csv_header() + csv_row(0,0,0,0,20) + "\n\n";
    REQUIRE(parse_csv(csv).ok());
}

TEST_CASE("CSV: min/max temperature computed correctly", "[csv][valid]") {
    const std::string csv = csv_header()
        + csv_row(0, 0,0,0,  10.0f)
        + csv_row(1, 1,0,0, 200.0f)
        + csv_row(2, 0,1,0,  50.0f);
    auto r = parse_csv(csv);
    REQUIRE(r.ok());
    CHECK(r.value().min_temp_C ==  10.0f);
    CHECK(r.value().max_temp_C == 200.0f);
}

TEST_CASE("CSV: temperature below -273.15 produces warning", "[csv][valid]") {
    const std::string csv = csv_header() + csv_row(0, 0,0,0, -300.0f);
    auto r = parse_csv(csv);
    REQUIRE(r.ok());
    CHECK(!r.value().warning.empty());
    CHECK(r.value().warning.find("-273.15") != std::string::npos);
}

// ═══════════════════════════════════════════════════════════════════════════════
// CSV MALFORMED
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("CSV: empty file returns error", "[csv][error]") {
    auto r = parse_csv("");
    REQUIRE(!r.ok());
    CHECK(!r.error().message.empty());
}

TEST_CASE("CSV: header-only returns error", "[csv][error]") {
    auto r = parse_csv(csv_header());
    REQUIRE(!r.ok());
    CHECK(!r.error().message.empty());
}

TEST_CASE("CSV: wrong header rejected with expected vs found", "[csv][error]") {
    const std::string csv = "node_id,x,y,z,temp\n0,0,0,0,25\n";
    auto r = parse_csv(csv);
    REQUIRE(!r.ok());
    INFO(r.error().message);
    CHECK(r.error().message.find("Expected") != std::string::npos);
    CHECK(r.error().message.find("Found") != std::string::npos);
}

TEST_CASE("CSV: duplicate node_id returns error with line number", "[csv][error]") {
    const std::string csv = csv_header()
        + csv_row(0, 0,0,0, 25)
        + csv_row(0, 1,0,0, 25);  // duplicate id 0
    auto r = parse_csv(csv);
    REQUIRE(!r.ok());
    INFO(r.error().message);
    CHECK(r.error().message.find("line ") != std::string::npos);
    CHECK(r.error().message.find("duplicate") != std::string::npos);
}

TEST_CASE("CSV: 6 columns returns error", "[csv][error]") {
    const std::string csv = csv_header() + "0,1,2,3,4,5\n";
    auto r = parse_csv(csv);
    REQUIRE(!r.ok());
    CHECK(r.error().message.find("line ") != std::string::npos);
}

TEST_CASE("CSV: text in numeric field returns error with line number", "[csv][error]") {
    const std::string csv = csv_header() + "0,abc,0,0,25\n";
    auto r = parse_csv(csv);
    REQUIRE(!r.ok());
    CHECK(r.error().message.find("line ") != std::string::npos);
}

TEST_CASE("CSV: NaN temperature returns error", "[csv][error]") {
    const std::string csv = csv_header() + "0,0,0,0,nan\n";
    auto r = parse_csv(csv);
    REQUIRE(!r.ok());
}

TEST_CASE("CSV: Inf coordinate returns error", "[csv][error]") {
    const std::string csv = csv_header() + "0,inf,0,0,25\n";
    auto r = parse_csv(csv);
    REQUIRE(!r.ok());
}

TEST_CASE("CSV: huge line (> 64 bytes per field) parses or errors cleanly", "[csv][error]") {
    // A huge number string that overflows float buffer.
    const std::string big_field(200, '1');
    const std::string csv = csv_header() + "0," + big_field + ",0,0,25\n";
    auto r = parse_csv(csv);
    REQUIRE(!r.ok());
    CHECK(!r.error().message.empty());
}

TEST_CASE("CSV: file over size cap returns error", "[csv][error]") {
    const std::string big(workbench::kMaxFileBytes + 1, '0');
    auto r = parse_csv(big);
    REQUIRE(!r.ok());
    CHECK(r.error().message.find("64 MiB") != std::string::npos);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Binary serialisation round-trip
// ═══════════════════════════════════════════════════════════════════════════════
TEST_CASE("STL: serialise_stl_bin round-trip size", "[stl][bin]") {
    auto r = parse_stl(one_triangle_stl());
    REQUIRE(r.ok());
    const auto bin = serialise_stl_bin(r.value());
    // 4 bytes n_tri + 9*4 bytes positions + 3*4 bytes normals
    CHECK(bin.size() == 4 + 9*4 + 3*4);
    // First 4 bytes = triangle count = 1 (little-endian)
    std::uint32_t n = 0;
    std::memcpy(&n, bin.data(), 4);
    CHECK(n == 1);
}

TEST_CASE("CSV: serialise_csv_bin round-trip size", "[csv][bin]") {
    const std::string csv = csv_header()
        + csv_row(0, 0,0,0, 20)
        + csv_row(1, 1,0,0, 30);
    auto r = parse_csv(csv);
    REQUIRE(r.ok());
    const auto bin = serialise_csv_bin(r.value());
    // 4 + 2*3*4 + 2*4 + 2*4 = 4 + 24 + 8 + 8 = 44
    CHECK(bin.size() == 4 + 2*3*4 + 2*4 + 2*4);
    std::uint32_t n = 0;
    std::memcpy(&n, bin.data(), 4);
    CHECK(n == 2);
}

// ═══════════════════════════════════════════════════════════════════════════════
// FUZZ-STYLE & BOUNDARY TEST SWEEP (Phase 8 Hardening)
// ═══════════════════════════════════════════════════════════════════════════════
#include <random>

TEST_CASE("Fuzz: random byte sequences feed to STL and CSV parsers assert no crash or hang", "[fuzz][geometry]") {
    std::mt19937_64 rng(0xC001CAFEULL); // Fixed deterministic seed
    std::uniform_int_distribution<int> byte_dist(0, 255);

    const std::vector<std::size_t> lengths = {0, 1, 2, 7, 16, 64, 128, 512, 1024, 4096, 16384};

    for (std::size_t len : lengths) {
        std::string junk;
        junk.reserve(len);
        for (std::size_t i = 0; i < len; ++i) {
            junk.push_back(static_cast<char>(byte_dist(rng)));
        }

        auto r_stl = parse_stl(junk);
        (void)r_stl;

        auto r_csv = parse_csv(junk);
        (void)r_csv;
    }
}

TEST_CASE("Fuzz: sliced & truncated valid files assert no crash or hang", "[fuzz][geometry]") {
    const std::string valid_stl = one_triangle_stl(0, 0, 0, 1, 0, 0, 0, 1, 0);
    const std::string valid_csv = csv_header()
        + csv_row(0, 0.0f, 0.0f, 0.0f, 100.0f)
        + csv_row(1, 0.5f, 0.0f, 0.0f, 75.0f)
        + csv_row(2, 1.0f, 0.0f, 0.0f, 50.0f);

    for (std::size_t i = 0; i <= valid_stl.size(); ++i) {
        std::string truncated = valid_stl.substr(0, i);
        auto r = parse_stl(truncated);
        // Assert no crash or hang; if failed, error message must not be empty
        if (!r.ok()) {
            CHECK(!r.error().message.empty());
        } else {
            CHECK(r.value().triangle_count <= 1);
        }
    }

    for (std::size_t i = 0; i <= valid_csv.size(); ++i) {
        std::string truncated = valid_csv.substr(0, i);
        auto r = parse_csv(truncated);
        // Assert no crash or hang; if failed, error message must not be empty
        if (!r.ok()) {
            CHECK(!r.error().message.empty());
        } else {
            CHECK(r.value().node_count >= 1);
        }
    }
}

TEST_CASE("Negative sweep: pathological whitespace and control characters", "[geometry][neg]") {
    using namespace std::string_literals;
    std::string null_in_stl = "solid test\n\0\0\0vertex 0 0 0\nendsolid"s;
    auto r1 = parse_stl(null_in_stl);
    CHECK(!r1.ok());

    std::string null_in_csv = "node_id,x_m,y_m,z_m,temperature_C\n0,\0,0,0,25\n"s;
    auto r2 = parse_csv(null_in_csv);
    CHECK(!r2.ok());

    CHECK(!parse_stl("   \n\t\r\n   ").ok());
    CHECK(!parse_csv("   \n\t\r\n   ").ok());
}
