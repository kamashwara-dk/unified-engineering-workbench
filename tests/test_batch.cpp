// test_batch.cpp -- Tests for multicore batch solver, generator, and benchmark harness.
#include "catch_amalgamated.hpp"
#include "workbench/batch.hpp"
#include "workbench/benchmark.hpp"
#include "workbench/platform.hpp"
#include <atomic>
#include <cmath>
#include <thread>

using namespace workbench;

TEST_CASE("BatchSpec validation rejects invalid configurations", "[batch]") {
    BatchSpec spec; // valid defaults

    SECTION("Valid default spec succeeds") {
        REQUIRE(validate_batch_spec(spec).ok());
    }

    SECTION("Zero case_count rejected") {
        spec.case_count = 0;
        auto st = validate_batch_spec(spec);
        REQUIRE_FALSE(st.ok());
        REQUIRE(st.code == ErrorCode::InvalidArgument);
    }

    SECTION("Invalid n_elements rejected") {
        spec.n_elements = 0;
        REQUIRE_FALSE(validate_batch_spec(spec).ok());
        spec.n_elements = -5;
        REQUIRE_FALSE(validate_batch_spec(spec).ok());
        spec.n_elements = kFem1dMaxElements + 1;
        REQUIRE_FALSE(validate_batch_spec(spec).ok());
    }

    SECTION("Negative or non-finite geometry rejected") {
        spec.length_m = -1.0;
        REQUIRE_FALSE(validate_batch_spec(spec).ok());
        spec.length_m = 1.0;

        spec.area_m2 = 0.0;
        REQUIRE_FALSE(validate_batch_spec(spec).ok());
        spec.area_m2 = 1e-4;
    }

    SECTION("Invalid thermal ranges rejected") {
        spec.k_min = -1.0;
        REQUIRE_FALSE(validate_batch_spec(spec).ok());
        spec.k_min = 10.0;
        spec.k_max = 5.0; // max < min
        REQUIRE_FALSE(validate_batch_spec(spec).ok());

        spec.k_min = 1.0; spec.k_max = 50.0;
        spec.q_min = -100.0;
        REQUIRE_FALSE(validate_batch_spec(spec).ok());
        spec.q_min = 1000.0;
        spec.q_max = 500.0; // max < min
        REQUIRE_FALSE(validate_batch_spec(spec).ok());
    }

    SECTION("Memory limit enforced") {
        // kMaxBatchNodalMemory = 50,000,000
        spec.case_count = 1'000'000;
        spec.n_elements = 100; // 1,000,000 * 101 = 101,000,000 > 50,000,000
        auto st = validate_batch_spec(spec);
        REQUIRE_FALSE(st.ok());
        REQUIRE(st.message.find("memory cap") != std::string::npos);
    }
}

TEST_CASE("Reproducible Case Generator", "[batch]") {
    BatchSpec spec;
    spec.seed = 42ULL;
    spec.case_count = 25;
    spec.n_elements = 10;

    SECTION("Same seed gives identical cases") {
        auto cases1 = generate_cases(spec);
        auto cases2 = generate_cases(spec);

        REQUIRE(cases1.size() == cases2.size());
        for (std::size_t i = 0; i < cases1.size(); ++i) {
            REQUIRE(cases1[i].conductivity_W_per_mK == cases2[i].conductivity_W_per_mK);
            REQUIRE(cases1[i].heat_generation_W_per_m3 == cases2[i].heat_generation_W_per_m3);
            REQUIRE(cases1[i].length_m == cases2[i].length_m);
            REQUIRE(cases1[i].area_m2 == cases2[i].area_m2);
            REQUIRE(cases1[i].T_left_C == cases2[i].T_left_C);
            REQUIRE(cases1[i].T_right_C == cases2[i].T_right_C);
            REQUIRE(cases1[i].n_elements == cases2[i].n_elements);
        }
    }

    SECTION("Different seeds differ") {
        BatchSpec spec2 = spec;
        spec2.seed = 9999ULL;

        auto cases1 = generate_cases(spec);
        auto cases2 = generate_cases(spec2);

        bool any_diff = false;
        for (std::size_t i = 0; i < cases1.size(); ++i) {
            if (cases1[i].conductivity_W_per_mK != cases2[i].conductivity_W_per_mK ||
                cases1[i].heat_generation_W_per_m3 != cases2[i].heat_generation_W_per_m3) {
                any_diff = true;
                break;
            }
        }
        REQUIRE(any_diff);
    }

    SECTION("Case i depends only on (seed, i), independent of scheduling") {
        auto all_cases = generate_cases(spec);
        for (std::size_t i : {0, 3, 7, 15, 24}) {
            auto single_case = generate_case(spec, i);
            REQUIRE(single_case.conductivity_W_per_mK == all_cases[i].conductivity_W_per_mK);
            REQUIRE(single_case.heat_generation_W_per_m3 == all_cases[i].heat_generation_W_per_m3);
        }
    }

    SECTION("Canonical bits-to-double mapping is in [0, 1)") {
        REQUIRE(bits_to_canonical_double(0ULL) == 0.0);
        // Maximum 64-bit uint
        const double max_val = bits_to_canonical_double(~0ULL);
        REQUIRE(max_val >= 0.0);
        REQUIRE(max_val < 1.0);
    }
}

TEST_CASE("Serial and Parallel agreement and order preservation", "[batch]") {
    BatchSpec spec;
    spec.seed = 2026ULL;
    spec.case_count = 30;
    spec.n_elements = 15;

    auto cases = generate_cases(spec);

    auto serial_res = run_serial(cases);
    REQUIRE(serial_res.ok());
    const auto& s_sols = serial_res.value();
    REQUIRE(s_sols.size() == cases.size());

    SECTION("Parallel with 2 workers exactly matches serial") {
        auto par2_res = run_parallel(cases, 2);
        REQUIRE(par2_res.ok());
        const auto& p_sols = par2_res.value();
        REQUIRE(p_sols.size() == cases.size());

        const double max_diff = compare_results(s_sols, p_sols);
        REQUIRE(max_diff <= 1e-8);
        REQUIRE(max_diff == 0.0); // exact numeric equality expected
    }

    SECTION("Parallel with 4 workers exactly matches serial") {
        auto par4_res = run_parallel(cases, 4);
        REQUIRE(par4_res.ok());
        const auto& p_sols = par4_res.value();
        REQUIRE(p_sols.size() == cases.size());

        const double max_diff = compare_results(s_sols, p_sols);
        REQUIRE(max_diff <= 1e-8);
        REQUIRE(max_diff == 0.0); // exact numeric equality expected
    }

    SECTION("Input order is strictly preserved in parallel results") {
        auto par_res = run_parallel(cases, 4);
        REQUIRE(par_res.ok());
        const auto& p_sols = par_res.value();

        for (std::size_t i = 0; i < cases.size(); ++i) {
            REQUIRE(p_sols[i].problem.conductivity_W_per_mK == cases[i].conductivity_W_per_mK);
            REQUIRE(p_sols[i].problem.heat_generation_W_per_m3 == cases[i].heat_generation_W_per_m3);
        }
    }
}

TEST_CASE("Worker thread pool caps and constraints", "[batch]") {
    SECTION("Worker counts are properly bounded") {
        REQUIRE(compute_effective_worker_count(1, 8) == 1);
        REQUIRE(compute_effective_worker_count(4, 8) == 4);
        REQUIRE(compute_effective_worker_count(16, 8) == 8); // capped at available
        REQUIRE(compute_effective_worker_count(100, 128) == 64); // capped at kMaxWorkerThreadCap (64)
        REQUIRE(compute_effective_worker_count(-2, 4) == 1); // floor at 1
    }

    SECTION("Single logical CPU path (injected logical cores = 1)") {
        BatchSpec spec;
        spec.case_count = 10;
        auto cases = generate_cases(spec);

        auto serial_res = run_serial(cases);
        REQUIRE(serial_res.ok());

        // Inject simulated_logical_cores = 1 with 2 requested workers
        auto par_res = run_parallel(cases, 2, {}, 1);
        REQUIRE(par_res.ok());
        REQUIRE(compare_results(serial_res.value(), par_res.value()) == 0.0);
    }
}

TEST_CASE("Cooperative cancellation mid-batch", "[batch]") {
    BatchSpec spec;
    spec.case_count = 200;
    spec.n_elements = 50;
    auto cases = generate_cases(spec);

    std::atomic<bool> cancel_flag{false};
    CancelToken token(&cancel_flag);

    // Cancel immediately before start
    cancel_flag.store(true);
    auto res = run_parallel(cases, 4, token);
    REQUIRE_FALSE(res.ok());
    REQUIRE(res.error().code == ErrorCode::Cancelled);
}

TEST_CASE("Hardware info and benchmark harness integration", "[benchmark]") {
    auto hw = get_hardware_info();
    REQUIRE_FALSE(hw.cpu_brand.empty());
    REQUIRE(hw.logical_cores >= 1);
    REQUIRE(hw.total_ram_bytes > 0);
    REQUIRE_FALSE(hw.compiler_name.empty());
    REQUIRE_FALSE(hw.build_type.empty());

    SECTION("Workload calibration returns advisory suggested count") {
        BatchSpec spec;
        spec.case_count = 50;
        auto calib = calibrate_workload(spec);
        REQUIRE(calib.probe_case_count == 50);
        REQUIRE(calib.suggested_case_count >= 50);
        REQUIRE_FALSE(calib.recommendation.empty());
    }

    SECTION("Single-CPU benchmark path marks speedup not assessable") {
        BatchSpec spec;
        spec.case_count = 10;
        spec.n_elements = 5;

        // Simulate 1 logical CPU
        auto rep_res = run_benchmark(spec, 150, 1);
        REQUIRE(rep_res.ok());
        const auto& report = rep_res.value();

        REQUIRE(report.launch_to_ready_ms == 150);
        REQUIRE(report.worker_results.size() >= 2);

        // 1 worker should be assessable
        REQUIRE(report.worker_results[0].worker_count == 1);
        REQUIRE(report.worker_results[0].speedup_assessable == true);

        // 2 workers path exercised for correctness but marked not assessable
        REQUIRE(report.worker_results[1].worker_count == 2);
        REQUIRE(report.worker_results[1].speedup_assessable == false);
        REQUIRE(report.worker_results[1].notes.find("not assessable") != std::string::npos);

        // Agreement check must still be exact
        REQUIRE(report.worker_results[1].max_diff_vs_serial <= 1e-8);
        REQUIRE(report.worker_results[1].max_diff_vs_serial == 0.0);
    }

    SECTION("Benchmark JSON and CSV serialization") {
        BatchSpec spec;
        spec.case_count = 10;
        spec.n_elements = 5;

        auto rep_res = run_benchmark(spec, 200, 2);
        REQUIRE(rep_res.ok());
        const auto& report = rep_res.value();

        const std::string json_out = benchmark_report_to_json(report);
        REQUIRE(json_out.find("\"cpu_brand\"") != std::string::npos);
        REQUIRE(json_out.find("\"worker_results\"") != std::string::npos);

        const std::string csv_out = benchmark_report_to_csv(report);
        REQUIRE(csv_out.find("worker_count,rep1_ms,rep2_ms") != std::string::npos);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// NEGATIVE & BOUNDARY TEST SWEEP (Phase 8 Hardening)
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Batch negative sweep: compare_results input validation", "[batch][neg]") {
    std::vector<RodSolution> sol_a;
    std::vector<RodSolution> sol_b;

    SECTION("Mismatched case counts throw invalid_argument") {
        RodSolution s1;
        s1.T_C = {100.0, 20.0};
        sol_a.push_back(s1);

        REQUIRE_THROWS_AS(compare_results(sol_a, sol_b), std::invalid_argument);
    }

    SECTION("Mismatched node counts throw invalid_argument") {
        RodSolution s1;
        s1.T_C = {100.0, 50.0, 20.0};
        RodSolution s2;
        s2.T_C = {100.0, 20.0};
        sol_a.push_back(s1);
        sol_b.push_back(s2);

        REQUIRE_THROWS_AS(compare_results(sol_a, sol_b), std::invalid_argument);
    }
}

TEST_CASE("Batch boundary sweep: empty cases vector and worker capping", "[batch][boundary]") {
    std::vector<RodProblem> empty_cases;

    auto s_res = run_serial(empty_cases);
    REQUIRE(s_res.ok());
    CHECK(s_res.value().empty());

    auto p_res = run_parallel(empty_cases, 4);
    REQUIRE(p_res.ok());
    CHECK(p_res.value().empty());

    CHECK(compute_effective_worker_count(0, 8) == 1);
    CHECK(compute_effective_worker_count(-1, 8) == 1);
    CHECK(compute_effective_worker_count(128, 8) == 8);
    CHECK(compute_effective_worker_count(2, 8) == 2);
}

