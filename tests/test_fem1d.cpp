// test_fem1d.cpp -- Phase 2 FEA oracle tests. Independent of the implementation.
// Oracle values from: midpoint error = (q/(8k)) * h^2 = 125 * h^2
// For q=1e4, k=10: constant = q/(8k) = 1e4/(80) = 125.
// Node order: 2.0 (linear elements are nodally exact for this problem).
#include "catch_amalgamated.hpp"
#include "workbench/fem1d.hpp"
#include <cmath>
#include <sstream>
#include <vector>

using namespace workbench;

// Default assignment problem.
static RodProblem default_problem() {
    return {1.0, 1e-4, 10.0, 1e4, 100.0, 20.0, 10};
}

// ── Oracle tests: N = 10, 20, 40, 80 ─────────────────────────────────────────
TEST_CASE("FEA oracle: midpoint error = 125*h^2 and nodal error < 1e-9", "[fem1d][oracle]") {
    const std::vector<int> ns = {10, 20, 40, 80};
    for (const int n : ns) {
        auto p = default_problem();
        p.n_elements = n;
        auto res = solve_rod(p);
        REQUIRE(res.ok());
        const auto& sol = res.value();

        const double h        = p.length_m / static_cast<double>(n);
        const double expected = 125.0 * h * h;

        INFO("N = " << n << "  h = " << h
             << "  mid_err = " << sol.max_midpoint_error()
             << "  expected = " << expected);

        CHECK(std::abs(sol.max_midpoint_error() - expected) < 1e-9);
        CHECK(sol.max_nodal_error() < 1e-9);

        // Boundary values exactly the BCs.
        CHECK(std::abs(sol.T_C.front() - 100.0) < 1e-14);
        CHECK(std::abs(sol.T_C.back()  -  20.0) < 1e-14);
    }
}

TEST_CASE("FEA oracle: observed convergence order = 2 within 0.01", "[fem1d][order]") {
    const std::vector<int> ns = {10, 20, 40, 80};
    auto base = default_problem();
    auto res  = run_convergence(base, ns);
    REQUIRE(res.ok());
    const auto& rows = res.value();
    REQUIRE(rows.size() == 4);

    // First row has NaN order (no predecessor).
    CHECK(std::isnan(rows[0].observed_order));
    // Remaining rows should be 2.0 +/- 0.01.
    for (std::size_t i = 1; i < rows.size(); ++i) {
        INFO("row " << i << " order = " << rows[i].observed_order);
        CHECK(std::abs(rows[i].observed_order - 2.0) < 0.01);
    }
}

// ── Equilibrium test ──────────────────────────────────────────────────────────
// R0 + RN = -q * A * L  (net external heat flux = minus total internal generation)
TEST_CASE("FEA equilibrium: R0 + RN = -q*A*L", "[fem1d][equilibrium]") {
    auto p   = default_problem();
    auto res = solve_rod(p);
    REQUIRE(res.ok());
    const auto& sol = res.value();

    const double expected = -p.heat_generation_W_per_m3 * p.area_m2 * p.length_m;
    const double sum      = sol.reaction_C_per_m[0] + sol.reaction_C_per_m[1];
    const double rel_err  = std::abs(sum - expected) / std::abs(expected);
    INFO("R0=" << sol.reaction_C_per_m[0] << " RN=" << sol.reaction_C_per_m[1]
         << " sum=" << sum << " expected=" << expected);
    CHECK(rel_err < 1e-9);
}

// ── N = 1 works ───────────────────────────────────────────────────────────────
TEST_CASE("FEA N=1 single element solves correctly", "[fem1d][n1]") {
    auto p = default_problem();
    p.n_elements = 1;
    auto res = solve_rod(p);
    REQUIRE(res.ok());
    const auto& sol = res.value();
    REQUIRE(sol.T_C.size() == 2);
    CHECK(std::abs(sol.T_C[0] - 100.0) < 1e-14);
    CHECK(std::abs(sol.T_C[1] -  20.0) < 1e-14);
}

// ── Different parameter set: generalised exact solution ───────────────────────
TEST_CASE("FEA generalised exact: different k, q, BCs", "[fem1d][generalised]") {
    RodProblem p;
    p.length_m               = 2.0;
    p.area_m2                = 5e-3;
    p.conductivity_W_per_mK  = 5.0;
    p.heat_generation_W_per_m3 = 2e3;
    p.T_left_C               = 50.0;
    p.T_right_C              = 10.0;
    p.n_elements             = 40;

    auto res = solve_rod(p);
    REQUIRE(res.ok());
    const auto& sol = res.value();

    const double h        = p.length_m / static_cast<double>(p.n_elements);
    const double expected = (p.heat_generation_W_per_m3 /
                             (8.0 * p.conductivity_W_per_mK)) * h * h;
    CHECK(std::abs(sol.max_midpoint_error() - expected) < 1e-9);
    CHECK(sol.max_nodal_error() < 1e-9);
}

// ── Negative tests: typed errors, no crash ────────────────────────────────────
TEST_CASE("FEA validation: k <= 0 returns InvalidArgument", "[fem1d][neg]") {
    auto p = default_problem();
    p.conductivity_W_per_mK = 0.0;
    auto res = solve_rod(p);
    CHECK(!res.ok());
    CHECK(res.error().code == ErrorCode::InvalidArgument);
}

TEST_CASE("FEA validation: A <= 0 returns InvalidArgument", "[fem1d][neg]") {
    auto p = default_problem(); p.area_m2 = -1.0;
    CHECK(!solve_rod(p).ok());
    CHECK(solve_rod(p).error().code == ErrorCode::InvalidArgument);
}

TEST_CASE("FEA validation: L <= 0 returns InvalidArgument", "[fem1d][neg]") {
    auto p = default_problem(); p.length_m = 0.0;
    CHECK(!solve_rod(p).ok());
}

TEST_CASE("FEA validation: NaN in length returns InvalidArgument", "[fem1d][neg]") {
    auto p = default_problem();
    p.length_m = std::numeric_limits<double>::quiet_NaN();
    CHECK(!solve_rod(p).ok());
    CHECK(solve_rod(p).error().code == ErrorCode::InvalidArgument);
}

TEST_CASE("FEA validation: Inf in k returns InvalidArgument", "[fem1d][neg]") {
    auto p = default_problem();
    p.conductivity_W_per_mK = std::numeric_limits<double>::infinity();
    CHECK(!solve_rod(p).ok());
}

TEST_CASE("FEA validation: N=0 returns InvalidArgument", "[fem1d][neg]") {
    auto p = default_problem(); p.n_elements = 0;
    CHECK(!solve_rod(p).ok());
    CHECK(solve_rod(p).error().code == ErrorCode::InvalidArgument);
}

TEST_CASE("FEA validation: N above maximum returns InvalidArgument", "[fem1d][neg]") {
    auto p = default_problem(); p.n_elements = kFem1dMaxElements + 1;
    CHECK(!solve_rod(p).ok());
    CHECK(solve_rod(p).error().code == ErrorCode::InvalidArgument);
}

TEST_CASE("FEA validation: NaN in T_left returns InvalidArgument", "[fem1d][neg]") {
    auto p = default_problem();
    p.T_left_C = std::numeric_limits<double>::quiet_NaN();
    CHECK(!solve_rod(p).ok());
}

// ── CSV output is well-formed ─────────────────────────────────────────────────
TEST_CASE("FEA CSV output has correct headers and rows", "[fem1d][csv]") {
    auto p = default_problem(); p.n_elements = 4;
    auto res = solve_rod(p);
    REQUIRE(res.ok());
    std::ostringstream oss;
    write_solution_csv(oss, res.value());
    const std::string csv = oss.str();
    CHECK(csv.find("N,x_m,T_fe_C,T_exact_C") != std::string::npos);
    // 4 elements -> 5 rows of data
    int row_count = 0;
    for (char c : csv) if (c == '\n') ++row_count;
    CHECK(row_count == 6); // header + 5 data rows
}

// ═══════════════════════════════════════════════════════════════════════════════
// NEGATIVE & BOUNDARY TEST SWEEP (Phase 8 Hardening)
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("FEA negative sweep: run_convergence boundary & invalid inputs", "[fem1d][neg][conv]") {
    auto p = default_problem();

    SECTION("Empty element count list returns InvalidArgument") {
        std::vector<int> empty_ns;
        auto res = run_convergence(p, empty_ns);
        REQUIRE_FALSE(res.ok());
        CHECK(res.error().code == ErrorCode::InvalidArgument);
    }

    SECTION("Non-positive element count in list returns InvalidArgument") {
        std::vector<int> bad_ns = {10, -5, 20};
        auto res = run_convergence(p, bad_ns);
        REQUIRE_FALSE(res.ok());
        CHECK(res.error().code == ErrorCode::InvalidArgument);
    }

    SECTION("Exceeding max elements in list returns InvalidArgument") {
        std::vector<int> huge_ns = {10, kFem1dMaxElements + 1};
        auto res = run_convergence(p, huge_ns);
        REQUIRE_FALSE(res.ok());
        CHECK(res.error().code == ErrorCode::InvalidArgument);
    }
}

TEST_CASE("FEA negative sweep: cooperative cancellation token", "[fem1d][cancel]") {
    auto p = default_problem();
    std::atomic<bool> cancelled{true};
    CancelToken token(&cancelled);

    auto res = solve_rod(p, token);
    REQUIRE_FALSE(res.ok());
    CHECK(res.error().code == ErrorCode::Cancelled);

    std::vector<int> ns = {10, 20};
    auto conv_res = run_convergence(p, ns, token);
    REQUIRE_FALSE(conv_res.ok());
    CHECK(conv_res.error().code == ErrorCode::Cancelled);
}

TEST_CASE("FEA negative sweep: non-finite boundary temperatures and heat generation", "[fem1d][neg]") {
    auto p = default_problem();

    p.T_right_C = std::numeric_limits<double>::infinity();
    CHECK_FALSE(solve_rod(p).ok());

    p.T_right_C = -std::numeric_limits<double>::infinity();
    CHECK_FALSE(solve_rod(p).ok());

    p.T_right_C = std::numeric_limits<double>::quiet_NaN();
    CHECK_FALSE(solve_rod(p).ok());

    p = default_problem();
    p.heat_generation_W_per_m3 = std::numeric_limits<double>::quiet_NaN();
    CHECK_FALSE(solve_rod(p).ok());
}
