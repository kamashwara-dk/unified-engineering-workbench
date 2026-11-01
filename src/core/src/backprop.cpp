// backprop.cpp -- 1-neuron non-linear backpropagation implementation (Phase 7).
#include "workbench/backprop.hpp"
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace workbench {
namespace {

bool are_params_finite(const BackpropParams& p) noexcept {
    return std::isfinite(p.w) && std::isfinite(p.b) &&
           std::isfinite(p.v) && std::isfinite(p.c);
}

bool are_inputs_finite(double x, double y, const BackpropParams& p) noexcept {
    return std::isfinite(x) && std::isfinite(y) && are_params_finite(p);
}

}  // namespace

Result<ForwardResult> forward(double x, double y, const BackpropParams& p) {
    if (!are_inputs_finite(x, y, p)) {
        return Status::invalid("forward: input parameters and coordinates must be finite numbers");
    }

    const double z     = p.w * x + p.b;
    const double h     = std::tanh(z);
    const double y_hat = p.v * h + p.c;
    const double diff  = y_hat - y;
    const double loss  = 0.5 * diff * diff;

    return ForwardResult{z, h, y_hat, loss};
}

// ─────────────────────────────────────────────────────────────────────────────
// analytic_gradients()
// NOTE: These analytical gradient formulas were provisially supplied by the
// AI agent per user request. The candidate's handwritten derivation is done
// separately and will be compared against this code. Kept in a single function
// for easy replacement.
// ─────────────────────────────────────────────────────────────────────────────
Result<BackpropGradients> analytic_gradients(double x, double y, const BackpropParams& p) {
    auto fwd = forward(x, y, p);
    if (!fwd.ok()) return fwd.error();

    const double diff   = fwd.value().y_hat - y;  // dJ/dy_hat = y_hat - y
    const double h      = fwd.value().h;
    const double v      = p.v;

    // Intermediate gradient with respect to hidden activation h:
    // dJ/dh = (y_hat - y) * v
    const double dJ_dh  = diff * v;

    // Gradient with respect to bias c:
    // dJ/dc = (y_hat - y) * 1 = y_hat - y
    const double dJ_dc  = diff;

    // Gradient with respect to weight v:
    // dJ/dv = (y_hat - y) * h
    const double dJ_dv  = diff * h;

    // dh/dz = 1 - tanh^2(z) = 1 - h^2
    const double dh_dz  = 1.0 - (h * h);

    // dJ/dz = dJ/dh * dh/dz = (y_hat - y) * v * (1 - h^2)
    const double dJ_dz  = dJ_dh * dh_dz;

    // Gradient with respect to weight w:
    // dJ/dw = dJ/dz * dz/dw = dJ/dz * x
    const double dJ_dw  = dJ_dz * x;

    // Gradient with respect to bias b:
    // dJ/db = dJ/dz * dz/db = dJ/dz * 1
    const double dJ_db  = dJ_dz;

    return BackpropGradients{dJ_dw, dJ_db, dJ_dv, dJ_dc, dJ_dh};
}

Result<BackpropGradients> numerical_gradients(double x, double y, const BackpropParams& p, double epsilon) {
    if (!are_inputs_finite(x, y, p) || !std::isfinite(epsilon) || epsilon <= 0.0) {
        return Status::invalid("numerical_gradients: inputs must be finite and epsilon > 0");
    }

    auto compute_loss = [&](const BackpropParams& pt) -> double {
        auto fr = forward(x, y, pt);
        return fr.ok() ? fr.value().loss : std::numeric_limits<double>::quiet_NaN();
    };

    // Numerical gradient for w
    BackpropParams pw_plus = p;  pw_plus.w += epsilon;
    BackpropParams pw_minus = p; pw_minus.w -= epsilon;
    const double dJ_dw_num = (compute_loss(pw_plus) - compute_loss(pw_minus)) / (2.0 * epsilon);

    // Numerical gradient for b
    BackpropParams pb_plus = p;  pb_plus.b += epsilon;
    BackpropParams pb_minus = p; pb_minus.b -= epsilon;
    const double dJ_db_num = (compute_loss(pb_plus) - compute_loss(pb_minus)) / (2.0 * epsilon);

    // Numerical gradient for v
    BackpropParams pv_plus = p;  pv_plus.v += epsilon;
    BackpropParams pv_minus = p; pv_minus.v -= epsilon;
    const double dJ_dv_num = (compute_loss(pv_plus) - compute_loss(pv_minus)) / (2.0 * epsilon);

    // Numerical gradient for c
    BackpropParams pc_plus = p;  pc_plus.c += epsilon;
    BackpropParams pc_minus = p; pc_minus.c -= epsilon;
    const double dJ_dc_num = (compute_loss(pc_plus) - compute_loss(pc_minus)) / (2.0 * epsilon);

    return BackpropGradients{dJ_dw_num, dJ_db_num, dJ_dv_num, dJ_dc_num, 0.0};
}

Result<GradientCheckResult> check_gradients(double x, double y, const BackpropParams& p, double epsilon, double tol) {
    auto an_res = analytic_gradients(x, y, p);
    if (!an_res.ok()) return an_res.error();

    auto num_res = numerical_gradients(x, y, p, epsilon);
    if (!num_res.ok()) return num_res.error();

    const auto& a = an_res.value();
    const auto& n = num_res.value();

    GradientCheckResult result;
    result.passed = true;

    auto check_entry = [&](const std::string& name, double analytic, double numerical) {
        const double err = std::abs(analytic - numerical);
        const bool ok = (err <= tol);
        if (!ok) {
            result.passed = false;
            if (result.failed_param.empty()) result.failed_param = name;
        }
        result.entries.push_back(GradientCheckEntry{name, analytic, numerical, err, ok});
    };

    check_entry("w", a.dJ_dw, n.dJ_dw);
    check_entry("b", a.dJ_db, n.dJ_db);
    check_entry("v", a.dJ_dv, n.dJ_dv);
    check_entry("c", a.dJ_dc, n.dJ_dc);

    return result;
}

Result<BackpropParams> step_simultaneous(double x, double y, const BackpropParams& p, double learning_rate) {
    if (!are_inputs_finite(x, y, p) || !std::isfinite(learning_rate) || learning_rate <= 0.0) {
        return Status::invalid("step_simultaneous: parameters and learning rate must be finite and lr > 0");
    }

    // Gradients evaluated simultaneously from the ORIGINAL parameters
    auto grad = analytic_gradients(x, y, p);
    if (!grad.ok()) return grad.error();

    BackpropParams p_next;
    p_next.w = p.w - learning_rate * grad.value().dJ_dw;
    p_next.b = p.b - learning_rate * grad.value().dJ_db;
    p_next.v = p.v - learning_rate * grad.value().dJ_dv;
    p_next.c = p.c - learning_rate * grad.value().dJ_dc;

    return p_next;
}

Result<std::vector<TrainingStepRecord>> train(
    double x, double y, const BackpropParams& initial_params, double learning_rate, int subsequent_steps)
{
    if (subsequent_steps < 0) {
        return Status::invalid("train: subsequent_steps must be non-negative");
    }

    std::vector<TrainingStepRecord> history;
    history.reserve(2 + static_cast<std::size_t>(subsequent_steps));

    // Step 0: Initial values
    auto fwd0 = forward(x, y, initial_params);
    if (!fwd0.ok()) return fwd0.error();

    history.push_back(TrainingStepRecord{
        0, fwd0.value().loss, initial_params.w, initial_params.b, initial_params.v, initial_params.c
    });

    // Step 1: After single simultaneous update (handwritten update benchmark)
    auto step1_res = step_simultaneous(x, y, initial_params, learning_rate);
    if (!step1_res.ok()) return step1_res.error();
    BackpropParams current_p = step1_res.value();

    auto fwd1 = forward(x, y, current_p);
    if (!fwd1.ok()) return fwd1.error();

    history.push_back(TrainingStepRecord{
        1, fwd1.value().loss, current_p.w, current_p.b, current_p.v, current_p.c
    });

    // Steps 2 through subsequent_steps + 1: 100 further steps
    for (int step = 2; step <= subsequent_steps + 1; ++step) {
        auto next_res = step_simultaneous(x, y, current_p, learning_rate);
        if (!next_res.ok()) return next_res.error();
        current_p = next_res.value();

        auto fwd = forward(x, y, current_p);
        if (!fwd.ok()) return fwd.error();

        history.push_back(TrainingStepRecord{
            step, fwd.value().loss, current_p.w, current_p.b, current_p.v, current_p.c
        });
    }

    return history;
}

std::string training_history_to_csv(const std::vector<TrainingStepRecord>& history) {
    std::ostringstream oss;
    oss.imbue(std::locale::classic());  // guarantees '.' decimal separator
    oss << std::setprecision(17);

    oss << "step,loss,w,b,v,c\n";
    for (const auto& r : history) {
        oss << r.step << ","
            << r.loss << ","
            << r.w    << ","
            << r.b    << ","
            << r.v    << ","
            << r.c    << "\n";
    }

    return oss.str();
}

}  // namespace workbench

