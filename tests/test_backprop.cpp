// test_backprop.cpp -- Unit tests for 1-neuron backpropagation and gradient check.
#include "catch_amalgamated.hpp"
#include "workbench/backprop.hpp"
#include <cmath>
#include <limits>
#include <string>

using namespace workbench;

TEST_CASE("Backprop: Initial forward pass matches analytical value", "[backprop][forward]") {
    BackpropParams p; // w=0.5, b=0.0, v=1.0, c=0.0
    const double x = 1.0;
    const double y = 2.0;

    auto res = forward(x, y, p);
    REQUIRE(res.ok());

    const auto& fwd = res.value();
    // z = 0.5 * 1 + 0 = 0.5
    CHECK(fwd.z == Catch::Approx(0.5));
    // h = tanh(0.5) ≈ 0.46211715726000976
    const double expected_h = std::tanh(0.5);
    CHECK(fwd.h == Catch::Approx(expected_h).epsilon(1e-12));
    // y_hat = 1.0 * h + 0 = h
    CHECK(fwd.y_hat == Catch::Approx(expected_h).epsilon(1e-12));
    // J = 0.5 * (y_hat - 2)^2
    const double expected_loss = 0.5 * (expected_h - 2.0) * (expected_h - 2.0);
    CHECK(fwd.loss == Catch::Approx(expected_loss).epsilon(1e-12));
}

TEST_CASE("Backprop: Central-difference gradient check passes at initial point (tol <= 1e-5)", "[backprop][gradcheck]") {
    BackpropParams p;
    const double x = 1.0;
    const double y = 2.0;
    const double eps = 1e-6;
    const double tol = 1e-5;

    auto check_res = check_gradients(x, y, p, eps, tol);
    REQUIRE(check_res.ok());

    const auto& report = check_res.value();
    INFO("Gradient check report:");
    for (const auto& entry : report.entries) {
        INFO("Param " << entry.param_name
             << ": analytic = " << entry.analytic_gradient
             << ", numerical = " << entry.numerical_gradient
             << ", abs_err = " << entry.absolute_error);
        CHECK(entry.passed);
        CHECK(entry.absolute_error <= tol);
    }

    CHECK(report.passed == true);
    CHECK(report.failed_param.empty());
    CHECK(report.entries.size() == 4);
}

TEST_CASE("Backprop: Gradient check stops and reports failure if analytic gradient is incorrect", "[backprop][gradcheck]") {
    BackpropParams p;
    const double x = 1.0;
    const double y = 2.0;

    auto an_res = analytic_gradients(x, y, p);
    REQUIRE(an_res.ok());
    auto num_res = numerical_gradients(x, y, p, 1e-6);
    REQUIRE(num_res.ok());

    // Introduce an intentional defect in one gradient (e.g. inverted sign or wrong factor)
    BackpropGradients bad_analytic = an_res.value();
    bad_analytic.dJ_dw *= -1.0; // corrupt w gradient

    GradientCheckResult report;
    report.passed = true;
    const double tol = 1e-5;

    auto check_param = [&](const std::string& name, double an, double num) {
        double err = std::abs(an - num);
        bool ok = (err <= tol);
        if (!ok) {
            report.passed = false;
            if (report.failed_param.empty()) report.failed_param = name;
        }
        report.entries.push_back({name, an, num, err, ok});
    };

    check_param("w", bad_analytic.dJ_dw, num_res.value().dJ_dw);
    check_param("b", bad_analytic.dJ_db, num_res.value().dJ_db);
    check_param("v", bad_analytic.dJ_dv, num_res.value().dJ_dv);
    check_param("c", bad_analytic.dJ_dc, num_res.value().dJ_dc);

    CHECK(report.passed == false);
    CHECK(report.failed_param == "w");
}

TEST_CASE("Backprop: One-step simultaneous update strictly uses original parameters", "[backprop][simultaneous]") {
    BackpropParams p; // initial: w=0.5, b=0.0, v=1.0, c=0.0
    const double x = 1.0;
    const double y = 2.0;
    const double lr = 0.1;

    // Simultaneous update via library
    auto sim_res = step_simultaneous(x, y, p, lr);
    REQUIRE(sim_res.ok());
    const BackpropParams p_sim = sim_res.value();

    // Verify analytical single-step updates:
    // grad_w ≈ -1.2094642436, grad_b ≈ -1.2094642436, grad_v ≈ -0.7106820547, grad_c ≈ -1.5378828427
    CHECK(p_sim.w == Catch::Approx(0.5 - lr * (-1.2094642436)).epsilon(1e-6));
    CHECK(p_sim.b == Catch::Approx(0.0 - lr * (-1.2094642436)).epsilon(1e-6));
    CHECK(p_sim.v == Catch::Approx(1.0 - lr * (-0.7106820547)).epsilon(1e-6));
    CHECK(p_sim.c == Catch::Approx(0.0 - lr * (-1.5378828427)).epsilon(1e-6));

    // Contrast with sequential update where w is updated before computing b:
    BackpropParams p_seq = p;
    auto grad1 = analytic_gradients(x, y, p_seq).value();
    p_seq.w -= lr * grad1.dJ_dw; // w changed!

    // Computing gradient for b with the new w:
    auto grad2 = analytic_gradients(x, y, p_seq).value();
    p_seq.b -= lr * grad2.dJ_db; // b updated using modified w!

    // The sequential update differs from simultaneous update:
    CHECK(p_seq.b != p_sim.b);
    CHECK(std::abs(p_seq.b - p_sim.b) > 1e-4);
}

TEST_CASE("Backprop: Training loss decreases monotonically for lr = 0.1 across 101 steps", "[backprop][train]") {
    BackpropParams p0;
    const double x = 1.0;
    const double y = 2.0;
    const double lr = 0.1;
    const int subsequent_steps = 100;

    auto history_res = train(x, y, p0, lr, subsequent_steps);
    REQUIRE(history_res.ok());

    const auto& history = history_res.value();
    // Step 0 + Step 1 + 100 further steps = 102 rows
    REQUIRE(history.size() == 102);

    CHECK(history[0].step == 0);
    CHECK(history[1].step == 1);
    CHECK(history.back().step == 101);

    // Initial loss is ~1.18255
    CHECK(history[0].loss == Catch::Approx(1.1825518).epsilon(1e-5));

    // Verify monotonic decrease for every single step
    for (std::size_t i = 1; i < history.size(); ++i) {
        INFO("Step " << history[i].step << ": loss = " << history[i].loss
             << ", prev_loss = " << history[i-1].loss);
        CHECK(history[i].loss < history[i-1].loss);
    }

    // Final loss after 101 steps is substantially reduced
    CHECK(history.back().loss < 0.005);
}

TEST_CASE("Backprop: Non-finite inputs return typed error", "[backprop][errors]") {
    const double nan_val = std::numeric_limits<double>::quiet_NaN();
    const double inf_val = std::numeric_limits<double>::infinity();

    BackpropParams p_nan = {nan_val, 0.0, 1.0, 0.0};
    CHECK(!forward(1.0, 2.0, p_nan).ok());
    CHECK(!analytic_gradients(1.0, 2.0, p_nan).ok());
    CHECK(!numerical_gradients(1.0, 2.0, p_nan).ok());
    CHECK(!step_simultaneous(1.0, 2.0, p_nan, 0.1).ok());

    BackpropParams p_inf = {0.5, inf_val, 1.0, 0.0};
    CHECK(!forward(1.0, 2.0, p_inf).ok());

    BackpropParams p_valid;
    CHECK(!forward(nan_val, 2.0, p_valid).ok());
    CHECK(!forward(1.0, nan_val, p_valid).ok());

    // Non-positive learning rate
    CHECK(!step_simultaneous(1.0, 2.0, p_valid, -0.1).ok());
    CHECK(!step_simultaneous(1.0, 2.0, p_valid, 0.0).ok());
    CHECK(!train(1.0, 2.0, p_valid, -0.1, 10).ok());
    CHECK(!train(1.0, 2.0, p_valid, 0.1, -5).ok());
}

TEST_CASE("Backprop: CSV export format produces valid header and full precision", "[backprop][csv]") {
    BackpropParams p0;
    auto history = train(1.0, 2.0, p0, 0.1, 5).value();
    std::string csv = training_history_to_csv(history);

    CHECK(csv.find("step,loss,w,b,v,c\n") == 0);
    // 1 header + 7 lines = 8 lines
    int lines = 0;
    for (char ch : csv) {
        if (ch == '\n') ++lines;
    }
    CHECK(lines == 8);
}

