#pragma once
// backprop.hpp -- 1-neuron non-linear backpropagation implementation (Phase 7).
//
// Model definition:
//   Input:  x = 1.0,  Target: y = 2.0
//   Hidden: z = w * x + b
//           h = tanh(z)
//   Output: y_hat = v * h + c
//   Loss:   J = 0.5 * (y_hat - y)^2
//
// Initial parameters: w = 0.5, b = 0.0, v = 1.0, c = 0.0
// Learning rate:      lr = 0.1
//
// NOTE: The analytical gradient formulas implemented in analytic_gradients()
// were provisially supplied by the AI agent per user request.
// The candidate's own handwritten derivation is done separately and will be
// compared against this implementation. The gradient check is an internal
// consistency check against central differences, not independent verification
// of the handwritten derivation.
#include "workbench/status.hpp"
#include <cmath>
#include <string>
#include <vector>

namespace workbench {

// Model parameter vector
struct BackpropParams {
    double w{0.5};
    double b{0.0};
    double v{1.0};
    double c{0.0};
};

// Gradients of loss J with respect to model parameters and intermediate h
struct BackpropGradients {
    double dJ_dw{0.0};
    double dJ_db{0.0};
    double dJ_dv{0.0};
    double dJ_dc{0.0};
    double dJ_dh{0.0};  // intermediate gradient: dJ/dh = (y_hat - y) * v
};

// Forward activation and loss values
struct ForwardResult {
    double z{0.0};       // w * x + b
    double h{0.0};       // tanh(z)
    double y_hat{0.0};   // v * h + c
    double loss{0.0};    // 0.5 * (y_hat - y)^2
};

// Logged record for training history
struct TrainingStepRecord {
    int    step{0};
    double loss{0.0};
    double w{0.0};
    double b{0.0};
    double v{0.0};
    double c{0.0};
};

// Individual parameter entry for gradient check report
struct GradientCheckEntry {
    std::string param_name;
    double analytic_gradient{0.0};
    double numerical_gradient{0.0};
    double absolute_error{0.0};
    bool   passed{false};
};

// Aggregated gradient check report
struct GradientCheckResult {
    bool passed{true};
    std::string failed_param; // empty if all passed
    std::vector<GradientCheckEntry> entries;
};

// Forward activation pass: evaluates z, h, y_hat, and loss J.
Result<ForwardResult> forward(double x, double y, const BackpropParams& p);

// Analytical gradients.
// KEPT IN A SINGLE ISOLATED FUNCTION so formulas can be easily inspected or replaced.
Result<BackpropGradients> analytic_gradients(double x, double y, const BackpropParams& p);

// Central-difference numerical gradients:
// Evaluates (J(param + eps) - J(param - eps)) / (2 * eps) for each parameter.
Result<BackpropGradients> numerical_gradients(double x, double y, const BackpropParams& p, double epsilon = 1e-6);

// Gradient check:
// Verifies absolute error <= tol (1e-5) between analytical and numerical gradients.
// If any check fails, returns passed = false and identifies failed_param with both values.
Result<GradientCheckResult> check_gradients(double x, double y, const BackpropParams& p, double epsilon = 1e-6, double tol = 1e-5);

// One simultaneous gradient descent step:
// Evaluates all gradients simultaneously from the ORIGINAL parameters, then updates w, b, v, c.
Result<BackpropParams> step_simultaneous(double x, double y, const BackpropParams& p, double learning_rate);

// Training run:
// step 0: initial parameters and loss
// step 1: after first simultaneous update
// steps 2..subsequent_steps+1: further simultaneous updates (e.g. 100 further steps => 102 total records)
Result<std::vector<TrainingStepRecord>> train(
    double x, double y, const BackpropParams& initial_params, double learning_rate, int subsequent_steps = 100);

// Serializes training history to CSV format:
// step,loss,w,b,v,c
// Written with full double precision and locale-independent formatting.
std::string training_history_to_csv(const std::vector<TrainingStepRecord>& history);

}  // namespace workbench

