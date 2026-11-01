#pragma once
// batch.hpp -- Parallel batch FEA solver and reproducible problem generation.
//
// Part of workbench_core (pure C++17). No HTTP, UI, or platform-specific APIs.
#include "workbench/fem1d.hpp"
#include "workbench/status.hpp"
#include <cstdint>
#include <vector>

namespace workbench {

// Documented memory limit: max total nodes across all cases in a batch.
// 50,000,000 nodes ~ 400 MB of double-precision nodal temperature storage.
inline constexpr std::size_t kMaxBatchNodalMemory = 50'000'000;

// Maximum thread pool size cap
inline constexpr int kMaxWorkerThreadCap = 64;

// ── Batch Specification ───────────────────────────────────────────────────────
struct BatchSpec {
    std::uint64_t seed                    {1337ULL}; // RNG seed
    std::size_t   case_count              {100};     // number of independent rod cases
    int           n_elements              {10};      // element count for all cases
    double        k_min                   {1.0};     // min thermal conductivity (W/(m·K)) > 0
    double        k_max                   {50.0};    // max thermal conductivity (W/(m·K)) >= k_min
    double        q_min                   {1000.0};  // min heat generation (W/m^3) > 0
    double        q_max                   {50000.0}; // max heat generation (W/m^3) >= q_min

    // Fixed rod geometry and boundary temperatures
    double        length_m                {1.0};     // length (m) > 0
    double        area_m2                 {1e-4};    // area (m^2) > 0
    double        T_left_C                {100.0};   // Dirichlet BC at x=0 (°C)
    double        T_right_C               {20.0};    // Dirichlet BC at x=L (°C)
};

// Validates all fields and enforces the memory cap.
[[nodiscard]] Status validate_batch_spec(const BatchSpec& spec);

// ── Raw-bits-to-double mapping ────────────────────────────────────────────────
// Deterministic canonical mapping: upper 53 bits multiplied by 2^-53.
// Produces a double uniformly in [0, 1) identically across all platforms and C++ runtimes.
[[nodiscard]] inline double bits_to_canonical_double(std::uint64_t x) noexcept {
    constexpr double kFactor = 1.0 / 9007199254740992.0; // 2^-53
    return static_cast<double>(x >> 11) * kFactor;
}

// ── Deterministic Case Generator ─────────────────────────────────────────────
// Generates case i such that case i depends strictly on (seed, i), independent
// of worker scheduling or thread assignment.
[[nodiscard]] RodProblem generate_case(const BatchSpec& spec, std::size_t case_index) noexcept;

// Generates all cases in input order.
[[nodiscard]] std::vector<RodProblem> generate_cases(const BatchSpec& spec);

// ── Solvers ───────────────────────────────────────────────────────────────────
// Serial batch runner: solves all cases in sequence using solve_rod.
[[nodiscard]] Result<std::vector<RodSolution>> run_serial(
    const std::vector<RodProblem>& cases,
    CancelToken                    cancel = {});

// Parallel batch runner: bounded std::thread pool, atomic next-index, preallocated
// result slots, input order preserved, cooperative cancellation.
// requested_workers is capped at min(requested_workers, hardware_concurrency, kMaxWorkerThreadCap).
// If max_hardware_concurrency > 0, it overrides std::thread::hardware_concurrency() (used for testing).
[[nodiscard]] Result<std::vector<RodSolution>> run_parallel(
    const std::vector<RodProblem>& cases,
    int                            requested_workers,
    CancelToken                    cancel = {},
    int                            max_hardware_concurrency = -1);

// ── Comparison ────────────────────────────────────────────────────────────────
// Compares two sets of solutions case-by-case and node-by-node.
// Returns maximum |dT| over all nodes of all cases.
// Throws std::invalid_argument if case counts or node counts differ.
[[nodiscard]] double compare_results(
    const std::vector<RodSolution>& a,
    const std::vector<RodSolution>& b);

// Helper to determine active worker count based on requested and available cores.
[[nodiscard]] int compute_effective_worker_count(int requested, int hardware_cores) noexcept;

}  // namespace workbench

