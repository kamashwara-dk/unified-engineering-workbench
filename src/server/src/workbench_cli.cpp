// workbench_cli.cpp -- Phase 2 & 7 CLI: run FEA or backpropagation and write CSVs.
// Usage: workbench_cli fem --out <relative_dir>
//        workbench_cli backprop --out <relative_dir>
// All paths are relative; no absolute machine paths written.
#include "workbench/fem1d.hpp"
#include "workbench/backprop.hpp"
#include "workbench/status.hpp"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

static void write_atomic(const std::string& path, const std::string& content) {
    const std::string tmp = path + ".tmp";
    {
        std::ofstream ofs(tmp, std::ios::binary | std::ios::trunc);
        if (!ofs) { std::fprintf(stderr, "ERROR: cannot write %s\n", tmp.c_str()); return; }
        ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    if (ec) std::fprintf(stderr, "ERROR: rename failed: %s\n", ec.message().c_str());
}

int main(int argc, char* argv[]) {
    std::string out_dir = "results";
    bool do_fem = false;
    bool do_backprop = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "fem") == 0)             do_fem = true;
        else if (std::strcmp(argv[i], "backprop") == 0)   do_backprop = true;
        else if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc)
            out_dir = argv[++i];
    }

    if (!do_fem && !do_backprop) {
        std::fprintf(stderr,
            "Usage: workbench_cli fem --out <relative_dir>\n"
            "       workbench_cli backprop --out <relative_dir>\n"
            "  Runs thermal FEA or 1-neuron backpropagation and writes CSVs.\n");
        return 1;
    }

    // Create output directory (relative path only).
    std::error_code ec;
    std::filesystem::create_directories(out_dir, ec);
    if (ec) {
        std::fprintf(stderr, "ERROR: cannot create directory %s: %s\n",
                     out_dir.c_str(), ec.message().c_str());
        return 1;
    }

    if (do_backprop) {
        workbench::BackpropParams p0; // w=0.5, b=0, v=1, c=0
        const double x = 1.0;
        const double y = 2.0;
        const double lr = 0.1;
        const int steps = 100;

        // Perform gradient check
        auto gcheck = workbench::check_gradients(x, y, p0, 1e-6, 1e-5);
        if (!gcheck.ok()) {
            std::fprintf(stderr, "ERROR gradient check: %s\n", gcheck.error().message.c_str());
            return 1;
        }

        std::fprintf(stdout, "=== Backpropagation Gradient Check (tol <= 1e-5) ===\n");
        for (const auto& e : gcheck.value().entries) {
            std::fprintf(stdout, "  param %s: analytic = %+.10f, numerical = %+.10f, diff = %.2e [%s]\n",
                         e.param_name.c_str(), e.analytic_gradient, e.numerical_gradient,
                         e.absolute_error, e.passed ? "PASS" : "FAIL");
        }
        if (!gcheck.value().passed) {
            std::fprintf(stderr, "ERROR: gradient check failed for param %s\n",
                         gcheck.value().failed_param.c_str());
            return 1;
        }

        // Run training: step 0 initial, step 1 single update, steps 2..101 100 further steps
        auto hist = workbench::train(x, y, p0, lr, steps);
        if (!hist.ok()) {
            std::fprintf(stderr, "ERROR training: %s\n", hist.error().message.c_str());
            return 1;
        }

        const std::string csv = workbench::training_history_to_csv(hist.value());
        const std::string path = out_dir + "/backprop.csv";
        write_atomic(path, csv);
        std::fprintf(stdout, "Written: %s (%zu rows)\n", path.c_str(), hist.value().size());
        return 0;
    }

    workbench::RodProblem base;
    // Assignment values (all defaults in RodProblem).

    const std::vector<int> ns = {10, 20, 40, 80};

    // Per-mesh solution CSVs.
    for (const int n : ns) {
        base.n_elements = n;
        auto res = workbench::solve_rod(base);
        if (!res.ok()) {
            std::fprintf(stderr, "ERROR N=%d: %s\n", n, res.error().message.c_str());
            return 1;
        }
        std::ostringstream oss;
        workbench::write_solution_csv(oss, res.value());
        const std::string path = out_dir + "/solution_N" + std::to_string(n) + ".csv";
        write_atomic(path, oss.str());
        std::fprintf(stdout, "Written: %s\n", path.c_str());
    }

    // Convergence CSV.
    auto conv = workbench::run_convergence(base, ns);
    if (!conv.ok()) {
        std::fprintf(stderr, "ERROR convergence: %s\n", conv.error().message.c_str());
        return 1;
    }
    std::ostringstream css;
    workbench::write_convergence_csv(css, conv.value());
    const std::string cpath = out_dir + "/convergence.csv";
    write_atomic(cpath, css.str());
    std::fprintf(stdout, "Written: %s\n", cpath.c_str());
    return 0;
}
