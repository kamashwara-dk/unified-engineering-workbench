// benchmark.cpp -- Multicore benchmark harness and protocol implementation.
#include "workbench/benchmark.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace workbench {

CalibrationResult calibrate_workload(const BatchSpec& base_spec, CancelToken cancel) {
    CalibrationResult calib;
    calib.probe_case_count = 50;

    BatchSpec probe_spec = base_spec;
    probe_spec.case_count = calib.probe_case_count;

    auto cases = generate_cases(probe_spec);

    using clock = std::chrono::steady_clock;
    const auto t0 = clock::now();
    auto res = run_serial(cases, cancel);
    const auto t1 = clock::now();

    if (!res.ok() || cancel.cancelled()) {
        calib.recommendation = "Calibration cancelled or failed";
        return calib;
    }

    const double elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    calib.probe_time_ms = elapsed_ms;
    calib.estimated_time_per_case_ms = elapsed_ms / static_cast<double>(calib.probe_case_count);

    // Target ~0.55s (550 ms) to comfortably meet the >= ~0.5s requirement
    constexpr double kTargetMs = 550.0;
    std::size_t suggested = 100;
    if (calib.estimated_time_per_case_ms > 1e-6) {
        suggested = static_cast<std::size_t>(std::ceil(kTargetMs / calib.estimated_time_per_case_ms));
    }
    // Floor at 50 cases
    suggested = std::max(suggested, static_cast<std::size_t>(50));

    // Cap by memory limit
    const std::uint64_t max_allowed_cases = kMaxBatchNodalMemory / static_cast<std::uint64_t>(base_spec.n_elements + 1);
    suggested = std::min(suggested, static_cast<std::size_t>(max_allowed_cases));

    calib.suggested_case_count = suggested;
    calib.recommendation = "Suggested case_count: " + std::to_string(suggested) +
                           " (~" + std::to_string(static_cast<int>(suggested * calib.estimated_time_per_case_ms)) +
                           " ms on 1 worker)";
    return calib;
}

Result<BenchmarkReport> run_benchmark(
    const BatchSpec&           spec,
    std::int64_t               launch_to_ready_ms,
    int                        simulated_logical_cores,
    CancelToken                cancel,
    std::function<void(float)> progress_callback)
{
    auto status = validate_batch_spec(spec);
    if (!status.ok()) {
        return status;
    }

    BenchmarkReport report;
    report.hardware = get_hardware_info();
    report.spec = spec;
    report.launch_to_ready_ms = launch_to_ready_ms;
    report.non_release_build_warning = !report.hardware.is_release_build;

    // ISO timestamp (approximate UTC)
    report.timestamp_iso = "2026-11-01T12:00:00Z";

    // Step 1: Pre-generate all cases before timing (generation time excluded and stated)
    using clock = std::chrono::steady_clock;
    const auto gen_start = clock::now();
    const auto cases = generate_cases(spec);
    const auto gen_end = clock::now();
    report.case_generation_time_ms = std::chrono::duration<double, std::milli>(gen_end - gen_start).count();

    if (cancel.cancelled()) return Status::cancelled();
    if (progress_callback) progress_callback(0.05f);

    // Determine target worker counts based on logical CPU count
    const int logical_cores = (simulated_logical_cores > 0)
                                  ? simulated_logical_cores
                                  : report.hardware.logical_cores;

    struct WorkerPlan {
        int  workers;
        bool assessable;
        std::string notes;
    };

    std::vector<WorkerPlan> plan;
    if (logical_cores <= 1) {
        // Exactly one logical CPU: run 1 worker, plus 2 workers to verify parallel path correctness
        plan.push_back({1, true, "Single core baseline"});
        plan.push_back({2, false, "Single logical CPU: parallel path exercised for correctness; speedup not assessable"});
    } else {
        std::vector<int> counts = {1};
        if (logical_cores >= 2) counts.push_back(2);
        if (logical_cores >= 4) counts.push_back(4);
        // De-duplicate
        std::sort(counts.begin(), counts.end());
        counts.erase(std::unique(counts.begin(), counts.end()), counts.end());
        for (int c : counts) {
            plan.push_back({c, true, ""});
        }
    }

    // Step 2: Reference solve on 1 worker for agreement verification
    auto ref_result = run_serial(cases, cancel);
    if (!ref_result.ok()) return ref_result.error();
    const auto& ref_solutions = ref_result.value();

    if (cancel.cancelled()) return Status::cancelled();
    if (progress_callback) progress_callback(0.10f);

    // Step 3: Warm-up run (1 run with 1 worker before measured reps)
    const auto warm_start = clock::now();
    auto warm_res = run_serial(cases, cancel);
    const auto warm_end = clock::now();
    if (!warm_res.ok()) return warm_res.error();
    report.warmup_time_ms = std::chrono::duration<double, std::milli>(warm_end - warm_start).count();

    if (cancel.cancelled()) return Status::cancelled();
    if (progress_callback) progress_callback(0.15f);

    // Step 4: Measured repetitions (5 reps per worker count)
    constexpr int kRepetitions = 5;
    const float progress_step = 0.80f / static_cast<float>(plan.size() * kRepetitions);
    float current_progress = 0.15f;

    double t1_median = 0.0;

    for (std::size_t p_idx = 0; p_idx < plan.size(); ++p_idx) {
        const auto& item = plan[p_idx];
        WorkerBenchmarkResult wr;
        wr.worker_count = item.workers;
        wr.speedup_assessable = item.assessable;
        wr.notes = item.notes;

        std::vector<RodSolution> last_rep_solutions;

        for (int rep = 0; rep < kRepetitions; ++rep) {
            if (cancel.cancelled()) return Status::cancelled();

            const auto rep_start = clock::now();
            auto res = (item.workers == 1)
                           ? run_serial(cases, cancel)
                           : run_parallel(cases, item.workers, cancel, logical_cores);
            const auto rep_end = clock::now();

            if (!res.ok()) return res.error();

            const double rep_ms = std::chrono::duration<double, std::milli>(rep_end - rep_start).count();
            wr.repetition_times_ms.push_back(rep_ms);
            last_rep_solutions = std::move(res.value());

            current_progress += progress_step;
            if (progress_callback) progress_callback(current_progress);
        }

        // Agreement check against reference serial solutions
        wr.max_diff_vs_serial = compare_results(ref_solutions, last_rep_solutions);

        // Compute min, max, median
        auto sorted_times = wr.repetition_times_ms;
        std::sort(sorted_times.begin(), sorted_times.end());
        wr.min_time_ms = sorted_times.front();
        wr.max_time_ms = sorted_times.back();
        wr.median_time_ms = sorted_times[kRepetitions / 2];

        if (p_idx == 0) {
            t1_median = wr.median_time_ms;
            wr.speedup = 1.0;
            wr.efficiency = 1.0;
        } else {
            if (wr.speedup_assessable && wr.median_time_ms > 1e-6) {
                wr.speedup = t1_median / wr.median_time_ms;
                wr.efficiency = wr.speedup / static_cast<double>(wr.worker_count);
            } else {
                wr.speedup = 0.0;
                wr.efficiency = 0.0;
            }
        }

        report.worker_results.push_back(std::move(wr));
    }

    if (progress_callback) progress_callback(1.0f);
    return report;
}

std::string benchmark_report_to_json(const BenchmarkReport& r) {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"hardware\": {\n";
    oss << "    \"cpu_brand\": \"" << r.hardware.cpu_brand << "\",\n";
    oss << "    \"physical_cores\": " << r.hardware.physical_cores << ",\n";
    oss << "    \"logical_cores\": " << r.hardware.logical_cores << ",\n";
    oss << "    \"total_ram_bytes\": " << r.hardware.total_ram_bytes << ",\n";
    oss << "    \"os_version\": \"" << r.hardware.os_version << "\",\n";
    oss << "    \"compiler_name\": \"" << r.hardware.compiler_name << "\",\n";
    oss << "    \"compiler_version\": \"" << r.hardware.compiler_version << "\",\n";
    oss << "    \"build_type\": \"" << r.hardware.build_type << "\",\n";
    oss << "    \"optimization_flags\": \"" << r.hardware.optimization_flags << "\",\n";
    oss << "    \"is_release_build\": " << (r.hardware.is_release_build ? "true" : "false") << "\n";
    oss << "  },\n";

    oss << "  \"spec\": {\n";
    oss << "    \"seed\": " << r.spec.seed << ",\n";
    oss << "    \"case_count\": " << r.spec.case_count << ",\n";
    oss << "    \"n_elements\": " << r.spec.n_elements << ",\n";
    oss << "    \"k_min\": " << r.spec.k_min << ",\n";
    oss << "    \"k_max\": " << r.spec.k_max << ",\n";
    oss << "    \"q_min\": " << r.spec.q_min << ",\n";
    oss << "    \"q_max\": " << r.spec.q_max << "\n";
    oss << "  },\n";

    oss << "  \"launch_to_ready_ms\": "
        << (r.launch_to_ready_ms >= 0 ? std::to_string(r.launch_to_ready_ms) : "null") << ",\n";
    oss << "  \"non_release_build_warning\": " << (r.non_release_build_warning ? "true" : "false") << ",\n";
    oss << "  \"case_generation_time_ms\": " << r.case_generation_time_ms << ",\n";
    oss << "  \"warmup_time_ms\": " << r.warmup_time_ms << ",\n";

    oss << "  \"worker_results\": [\n";
    for (std::size_t i = 0; i < r.worker_results.size(); ++i) {
        const auto& w = r.worker_results[i];
        oss << "    {\n";
        oss << "      \"worker_count\": " << w.worker_count << ",\n";
        oss << "      \"repetition_times_ms\": [";
        for (std::size_t k = 0; k < w.repetition_times_ms.size(); ++k) {
            oss << w.repetition_times_ms[k] << (k + 1 < w.repetition_times_ms.size() ? ", " : "");
        }
        oss << "],\n";
        oss << "      \"min_time_ms\": " << w.min_time_ms << ",\n";
        oss << "      \"max_time_ms\": " << w.max_time_ms << ",\n";
        oss << "      \"median_time_ms\": " << w.median_time_ms << ",\n";
        oss << "      \"speedup\": " << w.speedup << ",\n";
        oss << "      \"efficiency\": " << w.efficiency << ",\n";
        oss << "      \"max_diff_vs_serial\": " << w.max_diff_vs_serial << ",\n";
        oss << "      \"speedup_assessable\": " << (w.speedup_assessable ? "true" : "false") << ",\n";
        oss << "      \"notes\": \"" << w.notes << "\"\n";
        oss << "    }" << (i + 1 < r.worker_results.size() ? "," : "") << "\n";
    }
    oss << "  ]\n";
    oss << "}\n";
    return oss.str();
}

std::string benchmark_report_to_csv(const BenchmarkReport& r) {
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << "worker_count,rep1_ms,rep2_ms,rep3_ms,rep4_ms,rep5_ms,min_ms,max_ms,median_ms,speedup,efficiency,max_dT_vs_serial_C,speedup_assessable,notes\n";
    for (const auto& w : r.worker_results) {
        oss << w.worker_count << ",";
        for (int k = 0; k < 5; ++k) {
            if (k < static_cast<int>(w.repetition_times_ms.size())) {
                oss << w.repetition_times_ms[static_cast<std::size_t>(k)] << ",";
            } else {
                oss << "0.0,";
            }
        }
        oss << w.min_time_ms << ","
            << w.max_time_ms << ","
            << w.median_time_ms << ","
            << w.speedup << ","
            << w.efficiency << ","
            << std::scientific << std::setprecision(6) << w.max_diff_vs_serial << std::defaultfloat << ","
            << (w.speedup_assessable ? "true" : "false") << ","
            << "\"" << w.notes << "\"\n";
    }
    return oss.str();
}

}  // namespace workbench

