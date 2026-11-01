#pragma once
// fem1d.hpp -- 1-D thermal FEA rod problem.
//
// Problem: -d/dx(kA dT/dx) = qA, constant properties, insulated lateral surfaces.
// Two-node linear elements. Dirichlet BCs by symmetric elimination. Thomas solver.
// All units must use the names defined in the field identifiers (SI).
#include "workbench/status.hpp"
#include <cstdint>
#include <iosfwd>
#include <vector>

namespace workbench {

// Maximum number of elements. Kept moderate so memory stays bounded and the
// global matrix (N+1 nodes * 3 Thomas bands) is always stack-safe.
inline constexpr int kFem1dMaxElements = 100'000;

// ── Input ────────────────────────────────────────────────────────────────────
struct RodProblem {
    double length_m              {1.0};     // rod length (m), must be > 0
    double area_m2               {1e-4};    // cross-section area (m^2), must be > 0
    double conductivity_W_per_mK {10.0};    // thermal conductivity (W/(m·K)), must be > 0
    double heat_generation_W_per_m3{1e4};   // volumetric heat generation (W/m^3), finite
    double T_left_C              {100.0};   // Dirichlet BC at x=0 (°C), finite
    double T_right_C             {20.0};    // Dirichlet BC at x=L (°C), finite
    int    n_elements            {10};      // number of elements, in [1, kFem1dMaxElements]
};

// ── Output ───────────────────────────────────────────────────────────────────
struct RodSolution {
    RodProblem          problem;               // copy of the validated input
    std::vector<double> T_C;                   // nodal temperatures (°C), length n+1
    std::vector<double> reaction_C_per_m;      // [R0, RN] reaction forces (heat flux equiv.)

    // Returns the exact temperature at position x_m using the closed-form solution.
    [[nodiscard]] double exact_temperature(double x_m) const noexcept;

    // Linear FE interpolation at any x_m in [0, L].
    [[nodiscard]] double interpolate_fe(double x_m) const noexcept;

    // Max |T_fe(xi) - T_exact(xi)| over all interior nodes.
    [[nodiscard]] double max_nodal_error() const noexcept;

    // Max |T_fe(midpoint) - T_exact(midpoint)| over all elements.
    [[nodiscard]] double max_midpoint_error() const noexcept;
};

struct ConvergenceRow {
    int    n_elements;
    double h_m;              // element size
    double max_nodal_err_C;  // max nodal error (°C)
    double max_midpoint_err_C; // max midpoint error (°C)
    double observed_order;   // log2(e_prev / e_this), NaN for first row
};

// ── Validation ───────────────────────────────────────────────────────────────
// Returns Status::invalid() with a field-level message on failure.
[[nodiscard]] Status validate_rod_problem(const RodProblem& p);

// ── Solver ───────────────────────────────────────────────────────────────────
// Assemble, impose BCs, solve. Returns error on validation failure or near-zero pivot.
[[nodiscard]] Result<RodSolution> solve_rod(
    const RodProblem& problem,
    CancelToken       cancel = {});

// Run N = ns[0], ns[1], ... and collect convergence data.
[[nodiscard]] Result<std::vector<ConvergenceRow>> run_convergence(
    RodProblem                base_problem,
    const std::vector<int>&   n_values,
    CancelToken               cancel = {});

// ── CSV output ───────────────────────────────────────────────────────────────
// Writes locale-independent, full-precision CSV to any std::ostream.
void write_solution_csv(
    std::ostream&       out,
    const RodSolution&  sol);

void write_convergence_csv(
    std::ostream&               out,
    const std::vector<ConvergenceRow>& rows);

}  // namespace workbench
