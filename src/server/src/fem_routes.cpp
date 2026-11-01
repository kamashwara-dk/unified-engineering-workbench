// fem_routes.cpp -- FEM, batch, and benchmark job HTTP routes.
// POST /api/jobs {kind:"fem"|"batch"|"benchmark"|"calibrate", params:{...}}
// GET  /api/jobs/{id}
// DELETE /api/jobs/{id}
// GET  /api/jobs/{id}/export/{solution|convergence|benchmark}.csv
// GET  /api/jobs/{id}/export/benchmark.json
#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable: 4127 4244 4456 4702 4996)
#endif
#include "httplib.h"
#ifdef _MSC_VER
#  pragma warning(pop)
#endif
#include "json.hpp"
#include "workbench/job_manager.hpp"
#include "workbench/fem1d.hpp"
#include "workbench/batch.hpp"
#include "workbench/benchmark.hpp"
#include "workbench/backprop.hpp"
#include "workbench/platform.hpp"
#include "workbench/atomic_write.hpp"
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <string>

namespace workbench {

// Parse a RodProblem from JSON params object. Returns defaults for omitted fields.
static RodProblem parse_rod_params(const nlohmann::json& params) {
    RodProblem p; // defaults = assignment values
    auto get = [&](const char* key, double& out) {
        if (params.contains(key)) {
            if (!params[key].is_number())
                throw std::invalid_argument(std::string(key) + " must be a number");
            out = params[key].get<double>();
        }
    };
    auto get_int = [&](const char* key, int& out) {
        if (params.contains(key)) {
            if (!params[key].is_number_integer())
                throw std::invalid_argument(std::string(key) + " must be an integer");
            out = params[key].get<int>();
        }
    };
    get("length_m",                 p.length_m);
    get("area_m2",                  p.area_m2);
    get("conductivity_W_per_mK",    p.conductivity_W_per_mK);
    get("heat_generation_W_per_m3", p.heat_generation_W_per_m3);
    get("T_left_C",                 p.T_left_C);
    get("T_right_C",                p.T_right_C);
    get_int("n_elements",           p.n_elements);

    Status v = validate_rod_problem(p);
    if (!v.ok()) throw std::invalid_argument(v.message);
    return p;
}

// Parse a BatchSpec from JSON params object.
static BatchSpec parse_batch_params(const nlohmann::json& params) {
    BatchSpec s;
    auto get_double = [&](const char* key, double& out) {
        if (params.contains(key)) {
            if (!params[key].is_number())
                throw std::invalid_argument(std::string(key) + " must be a number");
            out = params[key].get<double>();
        }
    };
    auto get_u64 = [&](const char* key, std::uint64_t& out) {
        if (params.contains(key)) {
            if (!params[key].is_number_unsigned())
                throw std::invalid_argument(std::string(key) + " must be an unsigned integer");
            out = params[key].get<std::uint64_t>();
        }
    };
    auto get_size = [&](const char* key, std::size_t& out) {
        if (params.contains(key)) {
            if (!params[key].is_number_unsigned())
                throw std::invalid_argument(std::string(key) + " must be a positive integer");
            out = params[key].get<std::size_t>();
        }
    };
    auto get_int = [&](const char* key, int& out) {
        if (params.contains(key)) {
            if (!params[key].is_number_integer())
                throw std::invalid_argument(std::string(key) + " must be an integer");
            out = params[key].get<int>();
        }
    };

    get_u64("seed", s.seed);
    get_size("case_count", s.case_count);
    get_int("n_elements", s.n_elements);
    get_double("k_min", s.k_min);
    get_double("k_max", s.k_max);
    get_double("q_min", s.q_min);
    get_double("q_max", s.q_max);
    get_double("length_m", s.length_m);
    get_double("area_m2", s.area_m2);
    get_double("T_left_C", s.T_left_C);
    get_double("T_right_C", s.T_right_C);

    Status v = validate_batch_spec(s);
    if (!v.ok()) throw std::invalid_argument(v.message);
    return s;
}

static const char* state_str(JobState s) {
    switch (s) {
    case JobState::Queued:    return "queued";
    case JobState::Running:   return "running";
    case JobState::Succeeded: return "succeeded";
    case JobState::Failed:    return "failed";
    case JobState::Cancelled: return "cancelled";
    }
    return "unknown";
}

void register_fem_routes(
    httplib::Server&              svr,
    JobManager&                   jm,
    const std::string&            results_dir,
    std::function<std::int64_t()> get_ltr_fn)
{
    // POST /api/jobs
    svr.Post("/api/jobs", [&jm, results_dir, get_ltr_fn](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); }
        catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }

        if (!body.contains("kind") || !body["kind"].is_string()) {
            res.status = 400;
            res.set_content(R"({"error":"kind field is required"})", "application/json");
            return;
        }

        const std::string kind = body["kind"].get<std::string>();
        const nlohmann::json params = body.value("params", nlohmann::json::object());

        if (kind == "fem") {
            RodProblem problem;
            try { problem = parse_rod_params(params); }
            catch (const std::invalid_argument& ex) {
                nlohmann::json err;
                err["error"] = ex.what();
                res.status = 422;
                res.set_content(err.dump(), "application/json");
                return;
            }

            const std::string id = jm.submit("fem",
                [problem](CancelToken cancel, JobManager::ProgressFn progress) -> std::string {
                    progress(0.1f);
                    auto res = solve_rod(problem, cancel);
                    if (!res.ok()) throw std::runtime_error(res.error().message);
                    progress(0.4f);
                    const std::vector<int> ns = {10, 20, 40, 80};
                    auto conv_res = run_convergence(problem, ns, cancel);
                    if (!conv_res.ok()) throw std::runtime_error(conv_res.error().message);
                    progress(0.7f);

                    // Compute solutions for N = 10, 20, 40, 80
                    nlohmann::json sol_array = nlohmann::json::array();
                    for (int n : ns) {
                        RodProblem pn = problem;
                        pn.n_elements = n;
                        auto s = solve_rod(pn, cancel);
                        if (!s.ok()) throw std::runtime_error(s.error().message);
                        nlohmann::json sj;
                        sj["n_elements"] = n;
                        sj["h_m"] = pn.length_m / static_cast<double>(n);
                        std::vector<double> xs, t_fes, t_exs;
                        xs.reserve(n + 1);
                        t_fes.reserve(n + 1);
                        t_exs.reserve(n + 1);
                        for (int i = 0; i <= n; ++i) {
                            double x = i * (pn.length_m / static_cast<double>(n));
                            xs.push_back(x);
                            t_fes.push_back(s.value().T_C[i]);
                            t_exs.push_back(s.value().exact_temperature(x));
                        }
                        sj["x_m"] = xs;
                        sj["T_fe_C"] = t_fes;
                        sj["T_exact_C"] = t_exs;
                        sol_array.push_back(sj);
                    }

                    // Dense exact curve for smooth rendering
                    nlohmann::json exact_dense;
                    std::vector<double> dense_x, dense_t;
                    dense_x.reserve(101);
                    dense_t.reserve(101);
                    for (int i = 0; i <= 100; ++i) {
                        double x = problem.length_m * static_cast<double>(i) / 100.0;
                        dense_x.push_back(x);
                        dense_t.push_back(res.value().exact_temperature(x));
                    }
                    exact_dense["x_m"] = dense_x;
                    exact_dense["T_exact_C"] = dense_t;

                    // Convergence rows
                    nlohmann::json conv_array = nlohmann::json::array();
                    for (const auto& row : conv_res.value()) {
                        nlohmann::json rj;
                        rj["n_elements"] = row.n_elements;
                        rj["h_m"] = row.h_m;
                        rj["max_nodal_err_C"] = row.max_nodal_err_C;
                        rj["max_midpoint_err_C"] = row.max_midpoint_err_C;
                        if (std::isnan(row.observed_order)) {
                            rj["observed_order"] = nullptr;
                        } else {
                            rj["observed_order"] = row.observed_order;
                        }
                        conv_array.push_back(rj);
                    }

                    // Full CSVs
                    std::ostringstream sol_csv_ss;
                    write_solution_csv(sol_csv_ss, res.value());
                    std::ostringstream conv_csv_ss;
                    write_convergence_csv(conv_csv_ss, conv_res.value());

                    progress(1.0f);

                    nlohmann::json j;
                    j["n_elements"]         = problem.n_elements;
                    j["max_nodal_err_C"]    = res.value().max_nodal_error();
                    j["max_midpoint_err_C"] = res.value().max_midpoint_error();
                    j["convergence_rows"]   = conv_res.value().size();
                    j["convergence"]        = conv_array;
                    j["solutions"]          = sol_array;
                    j["exact_curve"]        = exact_dense;
                    j["solution_csv"]       = sol_csv_ss.str();
                    j["convergence_csv"]    = conv_csv_ss.str();
                    return j.dump();
                });

            nlohmann::json resp;
            resp["id"] = id;
            resp["state"] = "queued";
            res.status = 202;
            res.set_content(resp.dump(), "application/json");
            return;
        }

        if (kind == "batch") {
            BatchSpec spec;
            try { spec = parse_batch_params(params); }
            catch (const std::invalid_argument& ex) {
                nlohmann::json err;
                err["error"] = ex.what();
                res.status = 422;
                res.set_content(err.dump(), "application/json");
                return;
            }

            const int workers = params.value("workers", 0);

            const std::string id = jm.submit("batch",
                [spec, workers](CancelToken cancel, JobManager::ProgressFn progress) -> std::string {
                    progress(0.1f);
                    const auto cases = generate_cases(spec);
                    progress(0.3f);
                    auto s_res = run_serial(cases, cancel);
                    if (!s_res.ok()) throw std::runtime_error(s_res.error().message);
                    progress(0.6f);
                    auto p_res = run_parallel(cases, workers, cancel);
                    if (!p_res.ok()) throw std::runtime_error(p_res.error().message);
                    progress(0.9f);
                    const double diff = compare_results(s_res.value(), p_res.value());
                    progress(1.0f);

                    nlohmann::json j;
                    j["case_count"]         = spec.case_count;
                    j["n_elements"]         = spec.n_elements;
                    j["workers_requested"]  = workers;
                    j["max_diff_vs_serial"] = diff;
                    j["agreement_verified"] = (diff <= 1e-8);
                    return j.dump();
                });

            nlohmann::json resp;
            resp["id"] = id;
            resp["state"] = "queued";
            res.status = 202;
            res.set_content(resp.dump(), "application/json");
            return;
        }

        if (kind == "benchmark") {
            BatchSpec spec;
            try { spec = parse_batch_params(params); }
            catch (const std::invalid_argument& ex) {
                nlohmann::json err;
                err["error"] = ex.what();
                res.status = 422;
                res.set_content(err.dump(), "application/json");
                return;
            }

            const int simulated_cores = params.value("simulated_logical_cores", -1);
            const std::int64_t ltr = get_ltr_fn ? get_ltr_fn() : -1;

            const std::string id = jm.submit("benchmark",
                [spec, ltr, simulated_cores, results_dir, id_capture = std::string{}](
                    CancelToken cancel, JobManager::ProgressFn progress) mutable -> std::string {
                    auto rep_res = run_benchmark(spec, ltr, simulated_cores, cancel,
                        [&](float p) { progress(p); });
                    if (!rep_res.ok()) throw std::runtime_error(rep_res.error().message);

                    const auto& report = rep_res.value();
                    const std::string json_str = benchmark_report_to_json(report);
                    const std::string csv_str  = benchmark_report_to_csv(report);

                    // Atomic write results to results_dir
                    namespace fs = std::filesystem;
                    std::error_code ec;
                    fs::create_directories(results_dir, ec);

                    const std::string json_path = results_dir + "/benchmark_" + std::to_string(spec.seed) + ".json";
                    const std::string csv_path  = results_dir + "/benchmark_" + std::to_string(spec.seed) + ".csv";
                    (void)atomic_write_file(json_path, json_str);
                    (void)atomic_write_file(csv_path, csv_str);

                    nlohmann::json j = nlohmann::json::parse(json_str);
                    j["json_export_path"] = json_path;
                    j["csv_export_path"]  = csv_path;
                    j["json_data"]        = json_str;
                    j["csv_data"]         = csv_str;
                    return j.dump();
                });

            nlohmann::json resp;
            resp["id"] = id;
            resp["state"] = "queued";
            res.status = 202;
            res.set_content(resp.dump(), "application/json");
            return;
        }

        if (kind == "calibrate") {
            BatchSpec spec;
            try { spec = parse_batch_params(params); }
            catch (const std::invalid_argument& ex) {
                nlohmann::json err;
                err["error"] = ex.what();
                res.status = 422;
                res.set_content(err.dump(), "application/json");
                return;
            }

            const std::string id = jm.submit("calibrate",
                [spec](CancelToken cancel, JobManager::ProgressFn progress) -> std::string {
                    progress(0.2f);
                    auto calib = calibrate_workload(spec, cancel);
                    progress(1.0f);

                    nlohmann::json j;
                    j["probe_case_count"]           = calib.probe_case_count;
                    j["probe_time_ms"]              = calib.probe_time_ms;
                    j["estimated_time_per_case_ms"] = calib.estimated_time_per_case_ms;
                    j["suggested_case_count"]       = calib.suggested_case_count;
                    j["recommendation"]             = calib.recommendation;
                    return j.dump();
                });

            nlohmann::json resp;
            resp["id"] = id;
            resp["state"] = "queued";
            res.status = 202;
            res.set_content(resp.dump(), "application/json");
            return;
        }

        if (kind == "backprop") {
            double x = params.value("x", 1.0);
            double y = params.value("y", 2.0);
            BackpropParams p;
            p.w = params.value("w", 0.5);
            p.b = params.value("b", 0.0);
            p.v = params.value("v", 1.0);
            p.c = params.value("c", 0.0);
            double lr = params.value("learning_rate", 0.1);
            int subsequent_steps = params.value("subsequent_steps", 100);
            double epsilon = params.value("epsilon", 1e-6);
            double tolerance = params.value("tolerance", 1e-5);

            if (!std::isfinite(x) || !std::isfinite(y) ||
                !std::isfinite(p.w) || !std::isfinite(p.b) ||
                !std::isfinite(p.v) || !std::isfinite(p.c) ||
                !std::isfinite(lr) || lr <= 0.0 ||
                subsequent_steps < 0 || subsequent_steps > 10000 ||
                !std::isfinite(epsilon) || epsilon <= 0.0 ||
                !std::isfinite(tolerance) || tolerance <= 0.0) {
                nlohmann::json err;
                err["error"] = "Invalid parameters for backprop job";
                res.status = 422;
                res.set_content(err.dump(), "application/json");
                return;
            }

            const std::string id = jm.submit("backprop",
                [x, y, p, lr, subsequent_steps, epsilon, tolerance, results_dir](
                    CancelToken cancel, JobManager::ProgressFn progress) -> std::string {
                    progress(0.1f);
                    if (cancel.cancelled()) throw std::runtime_error("cancelled");

                    // 1. Gradient check
                    auto check_res = check_gradients(x, y, p, epsilon, tolerance);
                    if (!check_res.ok()) throw std::runtime_error(check_res.error().message);

                    const auto& gc = check_res.value();
                    nlohmann::json gc_json;
                    gc_json["passed"] = gc.passed;
                    gc_json["failed_param"] = gc.failed_param;
                    nlohmann::json entries = nlohmann::json::array();
                    for (const auto& entry : gc.entries) {
                        nlohmann::json ej;
                        ej["param_name"] = entry.param_name;
                        ej["analytic_gradient"] = entry.analytic_gradient;
                        ej["numerical_gradient"] = entry.numerical_gradient;
                        ej["absolute_error"] = entry.absolute_error;
                        ej["passed"] = entry.passed;
                        entries.push_back(ej);
                    }
                    gc_json["entries"] = entries;

                    if (!gc.passed) {
                        std::ostringstream ss;
                        ss << "Gradient check failed for " << gc.failed_param;
                        for (const auto& entry : gc.entries) {
                            if (entry.param_name == gc.failed_param) {
                                ss << " (analytic: " << entry.analytic_gradient
                                   << ", numerical: " << entry.numerical_gradient
                                   << ", abs error: " << entry.absolute_error << ")";
                                break;
                            }
                        }
                        throw std::runtime_error(ss.str());
                    }

                    progress(0.4f);
                    if (cancel.cancelled()) throw std::runtime_error("cancelled");

                    // 2. Training loop
                    auto train_res = train(x, y, p, lr, subsequent_steps);
                    if (!train_res.ok()) throw std::runtime_error(train_res.error().message);
                    const auto& history = train_res.value();

                    progress(0.8f);
                    if (cancel.cancelled()) throw std::runtime_error("cancelled");

                    const std::string csv_str = training_history_to_csv(history);

                    // Write results/backprop.csv
                    namespace fs = std::filesystem;
                    std::error_code ec;
                    fs::create_directories(results_dir, ec);
                    const std::string csv_path = results_dir + "/backprop.csv";
                    (void)atomic_write_file(csv_path, csv_str);

                    nlohmann::json j;
                    j["gradient_check"] = gc_json;
                    nlohmann::json hist_arr = nlohmann::json::array();
                    for (const auto& rec : history) {
                        nlohmann::json rj;
                        rj["step"] = rec.step;
                        rj["loss"] = rec.loss;
                        rj["w"] = rec.w;
                        rj["b"] = rec.b;
                        rj["v"] = rec.v;
                        rj["c"] = rec.c;
                        hist_arr.push_back(rj);
                    }
                    j["history"] = hist_arr;
                    j["csv_data"] = csv_str;
                    j["csv_export_path"] = csv_path;
                    progress(1.0f);
                    return j.dump();
                });

            nlohmann::json resp;
            resp["id"] = id;
            resp["state"] = "queued";
            res.status = 202;
            res.set_content(resp.dump(), "application/json");
            return;
        }

        res.status = 400;
        res.set_content(R"({"error":"unknown kind: must be \"fem\", \"batch\", \"benchmark\", \"calibrate\", or \"backprop\""})", "application/json");
    });

    // GET /api/jobs/:id
    svr.Get("/api/jobs/([a-f0-9]+)", [&jm](const httplib::Request& req, httplib::Response& res) {
        const std::string id = req.matches[1].str();
        auto result = jm.get(id);
        if (!result) {
            res.status = 404;
            res.set_content(R"({"error":"job not found"})", "application/json");
            return;
        }
        nlohmann::json j;
        j["id"]       = id;
        j["state"]    = state_str(result->state);
        j["progress"] = result->progress;
        if (!result->result_json.empty())  j["result"] = nlohmann::json::parse(result->result_json);
        if (!result->error_message.empty()) j["error"] = result->error_message;
        res.set_content(j.dump(), "application/json");
    });

    // DELETE /api/jobs/:id
    svr.Delete("/api/jobs/([a-f0-9]+)", [&jm](const httplib::Request& req, httplib::Response& res) {
        const std::string id = req.matches[1].str();
        if (!jm.get(id)) {
            res.status = 404;
            res.set_content(R"({"error":"job not found"})", "application/json");
            return;
        }
        jm.cancel(id);
        res.set_content(R"({"cancelled":true})", "application/json");
    });

    // GET /api/jobs/:id/export/solution.csv
    svr.Get("/api/jobs/([a-f0-9]+)/export/solution.csv",
        [&jm](const httplib::Request& req, httplib::Response& res) {
            const std::string id = req.matches[1].str();
            auto result = jm.get(id);
            if (!result || result->state != JobState::Succeeded) {
                res.status = 404;
                res.set_content(R"({"error":"job not complete"})", "application/json");
                return;
            }
            std::string csv;
            try {
                auto rj = nlohmann::json::parse(result->result_json);
                csv = rj.value("solution_csv", "");
            } catch (...) {}
            res.set_header("Content-Disposition", "attachment; filename=\"" + id + "_solution.csv\"");
            res.set_content(csv, "text/csv");
        });

    // GET /api/jobs/:id/export/convergence.csv
    svr.Get("/api/jobs/([a-f0-9]+)/export/convergence.csv",
        [&jm](const httplib::Request& req, httplib::Response& res) {
            const std::string id = req.matches[1].str();
            auto result = jm.get(id);
            if (!result || result->state != JobState::Succeeded) {
                res.status = 404;
                res.set_content(R"({"error":"job not complete"})", "application/json");
                return;
            }
            std::string csv;
            try {
                auto rj = nlohmann::json::parse(result->result_json);
                csv = rj.value("convergence_csv", "");
            } catch (...) {}
            res.set_header("Content-Disposition", "attachment; filename=\"" + id + "_convergence.csv\"");
            res.set_content(csv, "text/csv");
        });

    // GET /api/jobs/:id/export/benchmark.csv
    svr.Get("/api/jobs/([a-f0-9]+)/export/benchmark.csv",
        [&jm](const httplib::Request& req, httplib::Response& res) {
            const std::string id = req.matches[1].str();
            auto result = jm.get(id);
            if (!result || result->state != JobState::Succeeded) {
                res.status = 404;
                res.set_content(R"({"error":"job not complete"})", "application/json");
                return;
            }
            std::string csv;
            try {
                auto rj = nlohmann::json::parse(result->result_json);
                csv = rj.value("csv_data", "");
            } catch (...) {}
            res.set_header("Content-Disposition", "attachment; filename=\"" + id + "_benchmark.csv\"");
            res.set_content(csv, "text/csv");
        });

    // GET /api/jobs/:id/export/benchmark.json
    svr.Get("/api/jobs/([a-f0-9]+)/export/benchmark.json",
        [&jm](const httplib::Request& req, httplib::Response& res) {
            const std::string id = req.matches[1].str();
            auto result = jm.get(id);
            if (!result || result->state != JobState::Succeeded) {
                res.status = 404;
                res.set_content(R"({"error":"job not complete"})", "application/json");
                return;
            }
            std::string json;
            try {
                auto rj = nlohmann::json::parse(result->result_json);
                json = rj.value("json_data", "");
            } catch (...) {}
            res.set_header("Content-Disposition", "attachment; filename=\"" + id + "_benchmark.json\"");
            res.set_content(json, "application/json");
        });

    // GET /api/jobs/:id/export/backprop.csv
    svr.Get("/api/jobs/([a-f0-9]+)/export/backprop.csv",
        [&jm](const httplib::Request& req, httplib::Response& res) {
            const std::string id = req.matches[1].str();
            auto result = jm.get(id);
            if (!result || result->state != JobState::Succeeded) {
                res.status = 404;
                res.set_content(R"({"error":"job not complete"})", "application/json");
                return;
            }
            std::string csv;
            try {
                auto rj = nlohmann::json::parse(result->result_json);
                csv = rj.value("csv_data", "");
            } catch (...) {}
            res.set_header("Content-Disposition", "attachment; filename=\"" + id + "_backprop.csv\"");
            res.set_content(csv, "text/csv");
        });
}

}  // namespace workbench
