#pragma once

#include "core/types.hpp"
#include <vector>
#include <string>

namespace workbench::backprop {

/**
 * Neural regression network parameters.
 */
struct NetworkParameters {
    double w{0.5}; // Hidden weight
    double b{0.0}; // Hidden bias
    double v{1.0}; // Output weight
    double c{0.0}; // Output bias
};

/**
 * Gradients of the loss J with respect to each parameter.
 */
struct ParameterGradients {
    double dJ_dw{0.0};
    double dJ_db{0.0};
    double dJ_dv{0.0};
    double dJ_dc{0.0};
};

/**
 * Forward pass evaluation snapshot.
 */
struct ForwardPassResult {
    double x{1.0};
    double y{2.0};
    double z{0.0};     // z = w*x + b
    double h{0.0};     // h = tanh(z)
    double y_hat{0.0}; // y_hat = v*h + c
    double loss{0.0};  // J = 0.5 * (y_hat - y)^2
};

/**
 * Verification comparison between analytical gradient and central finite difference.
 */
struct GradientCheckEntry {
    std::string parameter_name;
    double analytical_grad{0.0};
    double numerical_grad{0.0};
    double absolute_error{0.0};
    bool passed{false}; // absolute_error <= 1e-5
};

/**
 * Complete numerical gradient check report for all 4 parameters.
 */
struct GradientCheckReport {
    double epsilon{1.0e-6};
    double tolerance{1.0e-5};
    std::vector<GradientCheckEntry> entries;
    bool all_passed{false};
};

/**
 * Step log for iterative training.
 */
struct TrainingStepRecord {
    int step{0};
    NetworkParameters params;
    ForwardPassResult forward;
    ParameterGradients gradients;
};

/**
 * Neural Backpropagation Engine.
 */
class BackpropEngine {
public:
    /**
     * Executes forward pass: h = tanh(wx + b), y_hat = vh + c, J = 0.5*(y_hat - y)^2
     */
    static ForwardPassResult forward(const NetworkParameters& params, double x = 1.0, double y = 2.0);

    /**
     * Evaluates loss J directly for given parameters.
     */
    static double compute_loss(const NetworkParameters& params, double x = 1.0, double y = 2.0);

    /**
     * Evaluates analytical gradients via chain rule (no automatic differentiation).
     */
    static ParameterGradients backward(const NetworkParameters& params, double x = 1.0, double y = 2.0);

    /**
     * Checks all initial analytical gradients against central finite differences with epsilon = 1e-6.
     */
    static GradientCheckReport verify_gradients_finite_difference(
        const NetworkParameters& initial_params = {0.5, 0.0, 1.0, 0.0},
        double x = 1.0,
        double y = 2.0,
        double epsilon = 1.0e-6,
        double tolerance = 1.0e-5
    );

    /**
     * Executes 100 subsequent training steps with learning rate eta = 0.1.
     */
    static std::vector<TrainingStepRecord> train_100_steps(
        NetworkParameters initial_params = {0.5, 0.0, 1.0, 0.0},
        double learning_rate = 0.1,
        double x = 1.0,
        double y = 2.0,
        int total_steps = 100
    );

    /**
     * Exports training loss history to CSV.
     */
    static std::string export_training_to_csv(const std::vector<TrainingStepRecord>& records);
};

} // namespace workbench::backprop
