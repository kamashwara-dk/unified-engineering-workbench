// fem1d.cpp -- 1-D thermal FEA implementation.
// Genuine FEA assembly: element loop, ke/fe scatter, Thomas solver.
// Do NOT shortcut by computing the answer from the closed-form expression.
#include "workbench/fem1d.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <ios>
#include <limits>
#include <ostream>
#include <sstream>
#include <stdexcept>

namespace workbench {

// ── Validation ───────────────────────────────────────────────────────────────
Status validate_rod_problem(const RodProblem& p) {
    // Field-level error messages reference the field by name.
    if (!std::isfinite(p.length_m) || p.length_m <= 0.0)
        return Status::invalid("length_m must be finite and > 0");
    if (!std::isfinite(p.area_m2) || p.area_m2 <= 0.0)
        return Status::invalid("area_m2 must be finite and > 0");
    if (!std::isfinite(p.conductivity_W_per_mK) || p.conductivity_W_per_mK <= 0.0)
        return Status::invalid("conductivity_W_per_mK must be finite and > 0");
    if (!std::isfinite(p.heat_generation_W_per_m3))
        return Status::invalid("heat_generation_W_per_m3 must be finite");
    if (!std::isfinite(p.T_left_C))
        return Status::invalid("T_left_C must be finite");
    if (!std::isfinite(p.T_right_C))
        return Status::invalid("T_right_C must be finite");
    if (p.n_elements < 1)
        return Status::invalid("n_elements must be >= 1");
    if (p.n_elements > kFem1dMaxElements)
        return Status::invalid("n_elements exceeds maximum (" +
            std::to_string(kFem1dMaxElements) + ")");
    return Status::success();
}

// ── Thomas (tridiagonal) solver ──────────────────────────────────────────────
// Solves a tridiagonal system in-place. a=sub, b=main, c=super, rhs=RHS.
// Solution written back to rhs. Near-zero pivot tolerance: 1e-15 * max|diag|.
static Status thomas_solve(
    std::vector<double>& a,
    std::vector<double>& b,
    const std::vector<double>& c,
    std::vector<double>& rhs)
{
    const int n = static_cast<int>(b.size());
    if (n == 0) return Status::success();
    double max_diag = 0.0;
    for (const double d : b) max_diag = std::max(max_diag, std::abs(d));
    const double pivot_tol = 1e-15 * (max_diag > 0.0 ? max_diag : 1.0);
    for (int i = 1; i < n; ++i) {
        if (std::abs(b[i - 1]) < pivot_tol)
            return Status::singular("near-zero pivot at row " + std::to_string(i-1));
        const double w = a[i] / b[i - 1];
        b[i]   -= w * c[i - 1];
        rhs[i] -= w * rhs[i - 1];
    }
    if (std::abs(b[n - 1]) < pivot_tol)
        return Status::singular("near-zero pivot at row " + std::to_string(n-1));
    rhs[n - 1] /= b[n - 1];
    for (int i = n - 2; i >= 0; --i)
        rhs[i] = (rhs[i] - c[i] * rhs[i + 1]) / b[i];
    return Status::success();
}

// ── Exact solution (generalised) ─────────────────────────────────────────────
// T(x) = T0 + (TL-T0)*x/L + (q/(2k))*x*(L-x)
static double exact_impl(double x, double L, double T0, double TL,
                          double q, double k) noexcept {
    return T0 + (TL - T0)*(x/L) + (q/(2.0*k))*x*(L - x);
}

// ── RodSolution methods ───────────────────────────────────────────────────────
double RodSolution::exact_temperature(double x_m) const noexcept {
    const auto& p = problem;
    return exact_impl(x_m, p.length_m, p.T_left_C, p.T_right_C,
                      p.heat_generation_W_per_m3, p.conductivity_W_per_mK);
}

double RodSolution::interpolate_fe(double x_m) const noexcept {
    const int    n  = problem.n_elements;
    const double h  = problem.length_m / static_cast<double>(n);
    const int    e  = std::min(static_cast<int>(x_m / h), n - 1);
    const double x0 = e * h;
    const double xi = (x_m - x0) / h;   // local coord [0,1]
    return (1.0 - xi) * T_C[e] + xi * T_C[e + 1];
}

double RodSolution::max_nodal_error() const noexcept {
    const int    n  = problem.n_elements;
    const double h  = problem.length_m / static_cast<double>(n);
    double max_err  = 0.0;
    for (int i = 0; i <= n; ++i) {
        const double x    = i * h;
        const double err  = std::abs(T_C[i] - exact_temperature(x));
        if (err > max_err) max_err = err;
    }
    return max_err;
}

double RodSolution::max_midpoint_error() const noexcept {
    const int    n  = problem.n_elements;
    const double h  = problem.length_m / static_cast<double>(n);
    double max_err  = 0.0;
    for (int e = 0; e < n; ++e) {
        const double xmid = (e + 0.5) * h;
        const double Tfe  = 0.5 * (T_C[e] + T_C[e + 1]);   // midpoint FE value
        const double err  = std::abs(Tfe - exact_temperature(xmid));
        if (err > max_err) max_err = err;
    }
    return max_err;
}

// ── solve_rod ─────────────────────────────────────────────────────────────────
Result<RodSolution> solve_rod(const RodProblem& p, CancelToken cancel) {
    {
        Status v = validate_rod_problem(p);
        if (!v.ok()) return v;
    }
    if (cancel.cancelled()) return Status::cancelled();

    const int    n   = p.n_elements;
    const int    nn  = n + 1;
    const double h   = p.length_m / static_cast<double>(n);
    const double kA  = p.conductivity_W_per_mK * p.area_m2;
    const double qAh2 = p.heat_generation_W_per_m3 * p.area_m2 * h / 2.0;

    // Global tridiagonal arrays (lower/main/upper/rhs), all zero-init.
    std::vector<double> a(nn, 0.0), b(nn, 0.0), c(nn, 0.0), f(nn, 0.0);

    // ── Genuine element loop: ke = (kA/h)*[[1,-1],[-1,1]], fe = qAh/2*[1,1]
    for (int e = 0; e < n; ++e) {
        const int i = e;
        const int j = e + 1;
        b[i] += kA / h;  c[i] -= kA / h;   // ke row i
        a[j] -= kA / h;  b[j] += kA / h;   // ke row j
        f[i] += qAh2;    f[j] += qAh2;      // fe scatter
    }

    if (cancel.cancelled()) return Status::cancelled();

    // ── Symmetric Dirichlet elimination ──────────────────────────────────────
    // Move T_left and T_right contributions to RHS, then replace BC rows.
    const double T0 = p.T_left_C, TL = p.T_right_C;
    if (nn > 1) { f[1]      -= a[1]      * T0; a[1]      = 0.0; }
    c[0] = 0.0; b[0] = 1.0; f[0] = T0;
    if (nn > 1) { f[nn - 2] -= c[nn - 2] * TL; c[nn - 2] = 0.0; }
    a[nn-1] = 0.0; b[nn-1] = 1.0; f[nn-1] = TL;

    // ── Thomas solve ─────────────────────────────────────────────────────────
    Status st = thomas_solve(a, b, c, f);
    if (!st.ok()) return st;

    // ── Reaction forces at boundary nodes ────────────────────────────────────
    // R = K_row * T_full - f_row  (original assembled values before elimination)
    // Node 0:  K_row = kA/h [1, -1, 0, ...],  f_row = qAh/2
    // Node N:  K_row = [..., 0, -1, 1] * kA/h, f_row = qAh/2
    const double kAh = kA / h;
    const double R0 =  kAh * (f[0]  - f[1])      - qAh2;
    const double RN = -kAh * (f[nn-2] - f[nn-1]) - qAh2;

    RodSolution sol;
    sol.problem           = p;
    sol.T_C               = std::move(f);
    sol.reaction_C_per_m  = {R0, RN};
    return sol;
}
// ── run_convergence ───────────────────────────────────────────────────────────
Result<std::vector<ConvergenceRow>> run_convergence(
    RodProblem base_problem,
    const std::vector<int>& n_values,
    CancelToken cancel)
{
    if (n_values.empty()) {
        return Status::invalid("n_values list cannot be empty");
    }
    std::vector<ConvergenceRow> rows;
    rows.reserve(n_values.size());
    double prev_midpoint_err = std::numeric_limits<double>::quiet_NaN();

    for (const int n : n_values) {
        if (cancel.cancelled()) return Status::cancelled();
        base_problem.n_elements = n;
        auto res = solve_rod(base_problem, cancel);
        if (!res.ok()) return res.error();

        const RodSolution& sol = res.value();
        const double h         = base_problem.length_m / static_cast<double>(n);
        const double mid_err   = sol.max_midpoint_error();
        const double nod_err   = sol.max_nodal_error();

        double order = std::numeric_limits<double>::quiet_NaN();
        if (std::isfinite(prev_midpoint_err) && mid_err > 0.0)
            order = std::log2(prev_midpoint_err / mid_err);

        rows.push_back({n, h, nod_err, mid_err, order});
        prev_midpoint_err = mid_err;
    }
    return rows;
}

// ── CSV writers ───────────────────────────────────────────────────────────────
// Locale-independent: imbue classic locale, full double precision.
static std::ostream& setup_csv(std::ostream& out) {
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<double>::max_digits10)
        << std::scientific;
    return out;
}

void write_solution_csv(std::ostream& out, const RodSolution& sol) {
    setup_csv(out);
    const int    n = sol.problem.n_elements;
    const double h = sol.problem.length_m / static_cast<double>(n);
    out << "N,x_m,T_fe_C,T_exact_C\n";
    for (int i = 0; i <= n; ++i) {
        const double x = i * h;
        out << n << ',' << x << ',' << sol.T_C[i]
            << ',' << sol.exact_temperature(x) << '\n';
    }
}

void write_convergence_csv(std::ostream& out,
                            const std::vector<ConvergenceRow>& rows) {
    setup_csv(out);
    out << "N,h_m,max_nodal_err_C,max_midpoint_err_C,observed_order\n";
    for (const auto& r : rows) {
        out << r.n_elements << ','
            << r.h_m << ','
            << r.max_nodal_err_C << ','
            << r.max_midpoint_err_C << ',';
        if (std::isnan(r.observed_order)) out << "NaN";
        else                               out << r.observed_order;
        out << '\n';
    }
}

}  // namespace workbench