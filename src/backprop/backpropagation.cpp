#include "backprop/backpropagation.hpp"
#include <cmath>
#include <sstream>
#include <iomanip>

namespace workbench::backprop {

ForwardPassResult BackpropEngine::forward(const NetworkParameters& params, double x, double y) {
    ForwardPassResult res;
    res.x = x;
    res.y = y;
    res.z = params.w * x + params.b;
    res.h = std::tanh(res.z);
    res.y_hat = params.v * res.h + params.c;
    double diff = res.y_hat - y;
    res.loss = 0.5 * diff * diff;
    return res;
}

double BackpropEngine::compute_loss(const NetworkParameters& params, double x, double y) {
    double z = params.w * x + params.b;
    double h = std::tanh(z);
    double y_hat = params.v * h + params.c;
    double diff = y_hat - y;
    return 0.5 * diff * diff;
}

ParameterGradients BackpropEngine::backward(const NetworkParameters& params, double x, double y) {
    ForwardPassResult fwd = forward(params, x, y);

    // 1. Output error gradient: dJ/dy_hat = (y_hat - y)
    double dJ_dyhat = fwd.y_hat - y;

    // 2. Output bias gradient: dJ/dc = (dJ/dy_hat) * (dy_hat/dc) = (y_hat - y) * 1
    double dJ_dc = dJ_dyhat * 1.0;

    // 3. Output weight gradient: dJ/dv = (dJ/dy_hat) * (dy_hat/dv) = (y_hat - y) * h
    double dJ_dv = dJ_dyhat * fwd.h;

    // 4. Hidden activation gradient: dJ/dh = (dJ/dy_hat) * (dy_hat/dh) = (y_hat - y) * v
    double dJ_dh = dJ_dyhat * params.v;

    // 5. Tanh derivative: dh/dz = 1 - tanh^2(z) = 1 - h^2
    double dh_dz = 1.0 - (fwd.h * fwd.h);

    // 6. Pre-activation gradient: dJ/dz = (dJ/dh) * (dh/dz)
    double dJ_dz = dJ_dh * dh_dz;

    // 7. Hidden bias gradient: dJ/db = (dJ/dz) * (dz/db) = dJ/dz * 1
    double dJ_db = dJ_dz * 1.0;

    // 8. Hidden weight gradient: dJ/dw = (dJ/dz) * (dz/dw) = dJ/dz * x
    double dJ_dw = dJ_dz * x;

    return ParameterGradients{ dJ_dw, dJ_db, dJ_dv, dJ_dc };
}

GradientCheckReport BackpropEngine::verify_gradients_finite_difference(
    const NetworkParameters& initial_params,
    double x,
    double y,
    double epsilon,
    double tolerance
) {
    GradientCheckReport report;
    report.epsilon = epsilon;
    report.tolerance = tolerance;
    report.all_passed = true;

    ParameterGradients analytical = backward(initial_params, x, y);

    // Numerical gradient helper: (J(theta + eps) - J(theta - eps)) / (2*eps)
    auto eval_num_grad = [&](auto mutate_fn) -> double {
        NetworkParameters p_plus = initial_params;
        NetworkParameters p_minus = initial_params;
        mutate_fn(p_plus, +epsilon);
        mutate_fn(p_minus, -epsilon);
        double loss_plus = compute_loss(p_plus, x, y);
        double loss_minus = compute_loss(p_minus, x, y);
        return (loss_plus - loss_minus) / (2.0 * epsilon);
    };

    // 1. w
    double num_w = eval_num_grad([](NetworkParameters& p, double d) { p.w += d; });
    double err_w = std::abs(analytical.dJ_dw - num_w);
    bool pass_w = (err_w <= tolerance);
    report.entries.push_back({ "w (hidden weight)", analytical.dJ_dw, num_w, err_w, pass_w });

    // 2. b
    double num_b = eval_num_grad([](NetworkParameters& p, double d) { p.b += d; });
    double err_b = std::abs(analytical.dJ_db - num_b);
    bool pass_b = (err_b <= tolerance);
    report.entries.push_back({ "b (hidden bias)", analytical.dJ_db, num_b, err_b, pass_b });

    // 3. v
    double num_v = eval_num_grad([](NetworkParameters& p, double d) { p.v += d; });
    double err_v = std::abs(analytical.dJ_dv - num_v);
    bool pass_v = (err_v <= tolerance);
    report.entries.push_back({ "v (output weight)", analytical.dJ_dv, num_v, err_v, pass_v });

    // 4. c
    double num_c = eval_num_grad([](NetworkParameters& p, double d) { p.c += d; });
    double err_c = std::abs(analytical.dJ_dc - num_c);
    bool pass_c = (err_c <= tolerance);
    report.entries.push_back({ "c (output bias)", analytical.dJ_dc, num_c, err_c, pass_c });

    for (const auto& entry : report.entries) {
        if (!entry.passed) {
            report.all_passed = false;
        }
    }

    return report;
}

std::vector<TrainingStepRecord> BackpropEngine::train_100_steps(
    NetworkParameters initial_params,
    double learning_rate,
    double x,
    double y,
    int total_steps
) {
    std::vector<TrainingStepRecord> history;
    history.reserve(total_steps + 1);

    NetworkParameters current_params = initial_params;

    for (int step = 0; step <= total_steps; ++step) {
        ForwardPassResult fwd = forward(current_params, x, y);
        ParameterGradients grads = backward(current_params, x, y);

        TrainingStepRecord rec;
        rec.step = step;
        rec.params = current_params;
        rec.forward = fwd;
        rec.gradients = grads;
        history.push_back(rec);

        if (step == total_steps) {
            break;
        }

        // Simultaneous gradient descent update using gradients of current step
        current_params.w -= learning_rate * grads.dJ_dw;
        current_params.b -= learning_rate * grads.dJ_db;
        current_params.v -= learning_rate * grads.dJ_dv;
        current_params.c -= learning_rate * grads.dJ_dc;
    }

    return history;
}

std::string BackpropEngine::export_training_to_csv(const std::vector<TrainingStepRecord>& records) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(8);
    ss << "step,w,b,v,c,z,h,y_hat,y_target,loss_J,dJ_dw,dJ_db,dJ_dv,dJ_dc\n";

    for (const auto& r : records) {
        ss << r.step << ","
           << r.params.w << "," << r.params.b << "," << r.params.v << "," << r.params.c << ","
           << r.forward.z << "," << r.forward.h << "," << r.forward.y_hat << "," << r.forward.y << ","
           << r.forward.loss << ","
           << r.gradients.dJ_dw << "," << r.gradients.dJ_db << "," << r.gradients.dJ_dv << "," << r.gradients.dJ_dc
           << "\n";
    }

    return ss.str();
}

} // namespace workbench::backprop

