// batch.cpp -- Parallel batch FEA solver implementation.
#include "workbench/batch.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>

namespace workbench {
namespace {

// SplitMix64 hash function for deterministic case seeding
inline std::uint64_t splitmix64(std::uint64_t x) noexcept {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

}  // namespace

Status validate_batch_spec(const BatchSpec& s) {
    if (s.case_count == 0) {
        return Status::invalid("case_count must be at least 1");
    }
    if (s.n_elements < 1 || s.n_elements > kFem1dMaxElements) {
        return Status::invalid("n_elements must be in [1, 100000]");
    }
    if (!std::isfinite(s.length_m) || s.length_m <= 0.0) {
        return Status::invalid("length_m must be positive and finite");
    }
    if (!std::isfinite(s.area_m2) || s.area_m2 <= 0.0) {
        return Status::invalid("area_m2 must be positive and finite");
    }
    if (!std::isfinite(s.k_min) || s.k_min <= 0.0) {
        return Status::invalid("k_min must be positive and finite");
    }
    if (!std::isfinite(s.k_max) || s.k_max < s.k_min) {
        return Status::invalid("k_max must be >= k_min and finite");
    }
    if (!std::isfinite(s.q_min) || s.q_min <= 0.0) {
        return Status::invalid("q_min must be positive and finite");
    }
    if (!std::isfinite(s.q_max) || s.q_max < s.q_min) {
        return Status::invalid("q_max must be >= q_min and finite");
    }
    if (!std::isfinite(s.T_left_C)) {
        return Status::invalid("T_left_C must be finite");
    }
    if (!std::isfinite(s.T_right_C)) {
        return Status::invalid("T_right_C must be finite");
    }

    const std::uint64_t total_nodes = static_cast<std::uint64_t>(s.case_count) *
                                      static_cast<std::uint64_t>(s.n_elements + 1);
    if (total_nodes > kMaxBatchNodalMemory) {
        return Status::invalid(
            "Batch memory cap exceeded: requested " + std::to_string(total_nodes) +
            " nodes, maximum allowed is " + std::to_string(kMaxBatchNodalMemory));
    }

    return Status::success();
}

RodProblem generate_case(const BatchSpec& spec, std::size_t case_index) noexcept {
    // Deterministic state derivation from (seed, case_index)
    const std::uint64_t case_seed = splitmix64(spec.seed ^ splitmix64(static_cast<std::uint64_t>(case_index)));
    std::mt19937_64 rng(case_seed);

    const double u_k = bits_to_canonical_double(rng());
    const double u_q = bits_to_canonical_double(rng());

    RodProblem p;
    p.length_m = spec.length_m;
    p.area_m2 = spec.area_m2;
    p.conductivity_W_per_mK = spec.k_min + u_k * (spec.k_max - spec.k_min);
    p.heat_generation_W_per_m3 = spec.q_min + u_q * (spec.q_max - spec.q_min);
    p.T_left_C = spec.T_left_C;
    p.T_right_C = spec.T_right_C;
    p.n_elements = spec.n_elements;
    return p;
}

std::vector<RodProblem> generate_cases(const BatchSpec& spec) {
    std::vector<RodProblem> cases;
    cases.reserve(spec.case_count);
    for (std::size_t i = 0; i < spec.case_count; ++i) {
        cases.push_back(generate_case(spec, i));
    }
    return cases;
}

int compute_effective_worker_count(int requested, int hardware_cores) noexcept {
    if (requested < 1) requested = 1;
    int available = hardware_cores;
    if (available <= 0) {
        available = static_cast<int>(std::thread::hardware_concurrency());
        if (available <= 0) available = 1;
    }
    return std::max(1, std::min({requested, available, kMaxWorkerThreadCap}));
}

Result<std::vector<RodSolution>> run_serial(
    const std::vector<RodProblem>& cases,
    CancelToken                    cancel)
{
    std::vector<RodSolution> solutions;
    solutions.reserve(cases.size());

    for (const auto& problem : cases) {
        if (cancel.cancelled()) {
            return Status::cancelled();
        }
        auto res = solve_rod(problem, cancel);
        if (!res.ok()) {
            return res.error();
        }
        solutions.push_back(std::move(res.value()));
    }

    return solutions;
}

Result<std::vector<RodSolution>> run_parallel(
    const std::vector<RodProblem>& cases,
    int                            requested_workers,
    CancelToken                    cancel,
    int                            max_hardware_concurrency)
{
    if (cases.empty()) {
        return std::vector<RodSolution>{};
    }

    const int worker_count = compute_effective_worker_count(
        requested_workers,
        max_hardware_concurrency);

    if (worker_count <= 1 || cases.size() <= 1) {
        return run_serial(cases, cancel);
    }

    // Preallocated result slots written by index only (deterministic input-order output,
    // no locks on hot path).
    std::vector<RodSolution> solutions(cases.size());
    std::atomic<std::size_t> next_index{0};
    std::mutex err_mutex;
    Status first_error = Status::success();

    auto worker_fn = [&]() {
        while (!cancel.cancelled()) {
            const std::size_t idx = next_index.fetch_add(1, std::memory_order_relaxed);
            if (idx >= cases.size()) {
                break;
            }
            try {
                auto res = solve_rod(cases[idx], cancel);
                if (!res.ok()) {
                    std::lock_guard<std::mutex> lock(err_mutex);
                    if (first_error.ok()) first_error = res.error();
                } else {
                    solutions[idx] = std::move(res.value());
                }
            } catch (const std::exception& ex) {
                std::lock_guard<std::mutex> lock(err_mutex);
                if (first_error.ok()) first_error = Status::invalid(ex.what());
            } catch (...) {
                std::lock_guard<std::mutex> lock(err_mutex);
                if (first_error.ok()) first_error = Status::internal("Unknown worker exception");
            }
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(static_cast<std::size_t>(worker_count));
    for (int i = 0; i < worker_count; ++i) {
        threads.emplace_back(worker_fn);
    }

    for (auto& th : threads) {
        if (th.joinable()) {
            th.join();
        }
    }

    if (cancel.cancelled()) {
        return Status::cancelled();
    }

    if (!first_error.ok()) {
        return first_error;
    }

    return solutions;
}

double compare_results(
    const std::vector<RodSolution>& a,
    const std::vector<RodSolution>& b)
{
    if (a.size() != b.size()) {
        throw std::invalid_argument("Case count mismatch in compare_results");
    }

    double max_diff = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const auto& sol_a = a[i];
        const auto& sol_b = b[i];
        if (sol_a.T_C.size() != sol_b.T_C.size()) {
            throw std::invalid_argument("Node count mismatch in case " + std::to_string(i));
        }
        for (std::size_t j = 0; j < sol_a.T_C.size(); ++j) {
            const double diff = std::abs(sol_a.T_C[j] - sol_b.T_C[j]);
            if (diff > max_diff) {
                max_diff = diff;
            }
        }
    }
    return max_diff;
}

}  // namespace workbench
