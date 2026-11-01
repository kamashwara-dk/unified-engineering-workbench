#pragma once
// benchmark.hpp -- Multicore benchmark harness, protocol, and calibration.
//
// Part of workbench_platform (separate from pure C++ core).
#include "workbench/batch.hpp"
#include "workbench/platform.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace workbench {

struct WorkerBenchmarkResult {
    int                 worker_count           {1};
    std::vector<double> repetition_times_ms;            // 5 measured repetition times (ms)
    double              min_time_ms            {0.0};
    double              max_time_ms            {0.0};
    double              median_time_ms         {0.0};
    double              speedup                {1.0};   // T1 / Tp
    double              efficiency             {1.0};   // (T1 / Tp) / p
    double              max_diff_vs_serial     {0.0};   // agreement check against serial
    bool                speedup_assessable     {true};  // false if only 1 logical core
    std::string         notes;
};

struct BenchmarkReport {
    HardwareInfo                       hardware;
    BatchSpec                          spec;
    std::int64_t                       launch_to_ready_ms        {-1};
    bool                               non_release_build_warning {false};
    double                             case_generation_time_ms   {0.0}; // stated as excluded
    double                             warmup_time_ms            {0.0};
    std::vector<WorkerBenchmarkResult> worker_results;
    std::string                        timestamp_iso;
};

struct CalibrationResult {
    std::size_t probe_case_count           {50};
    double      probe_time_ms              {0.0};
    double      estimated_time_per_case_ms {0.0};
    std::size_t suggested_case_count       {100};
    std::string recommendation;
};

// Suggests a case_count such that 1 repetition on 1 worker takes at least ~0.5s.
[[nodiscard]] CalibrationResult calibrate_workload(
    const BatchSpec& base_spec,
    CancelToken      cancel = {});

// Executes the complete benchmark protocol:
// 1. Generation of cases before timing.
// 2. 1 warm-up run.
// 3. 5 measured runs per worker count (1, 2, 4 capped at logical cores).
// 4. Agreement check against serial run (max |dT| <= 1e-8 °C).
// 5. Speedup and efficiency calculations.
[[nodiscard]] Result<BenchmarkReport> run_benchmark(
    const BatchSpec&           spec,
    std::int64_t               launch_to_ready_ms = -1,
    int                        simulated_logical_cores = -1, // for testing single-CPU path
    CancelToken                cancel = {},
    std::function<void(float)> progress_callback = {});

// Serializes report to formatted JSON and CSV strings.
[[nodiscard]] std::string benchmark_report_to_json(const BenchmarkReport& report);
[[nodiscard]] std::string benchmark_report_to_csv(const BenchmarkReport& report);

}  // namespace workbench

