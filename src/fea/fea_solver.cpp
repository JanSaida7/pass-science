#include "fea/fea_solver.hpp"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace workbench::fea {

double ThermalFeaSolver::exact_temperature(double x, const ThermalProblemConfig& config) {
    double L = config.length_m;
    double k = config.conductivity_k;
    double q = config.heat_gen_q;
    double T0 = config.temp_left_C;
    double TL = config.temp_right_C;

    // Linear conduction drop term + parabolic internal heat generation term
    double linear_term = T0 + (TL - T0) * (x / L);
    double generation_term = (q / (2.0 * k)) * x * (L - x);
    return linear_term + generation_term;
}

core::Result<FeaSolution> ThermalFeaSolver::solve(const ThermalProblemConfig& config) {
    if (config.length_m <= 0.0) {
        return core::Result<FeaSolution>::Fail("Rod length L must be positive, got: " + std::to_string(config.length_m));
    }
    if (config.area_m2 <= 0.0) {
        return core::Result<FeaSolution>::Fail("Cross-sectional area A must be positive, got: " + std::to_string(config.area_m2));
    }
    if (config.conductivity_k <= 0.0) {
        return core::Result<FeaSolution>::Fail("Thermal conductivity k must be positive, got: " + std::to_string(config.conductivity_k));
    }
    if (config.element_count < 1) {
        return core::Result<FeaSolution>::Fail("Element count N must be >= 1, got: " + std::to_string(config.element_count));
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    const int N = config.element_count;
    const int num_nodes = N + 1;
    const double L = config.length_m;
    const double A = config.area_m2;
    const double k = config.conductivity_k;
    const double q = config.heat_gen_q;
    const double h = L / static_cast<double>(N);

    // Node coordinates
    std::vector<double> x(num_nodes);
    for (int i = 0; i < num_nodes; ++i) {
        x[i] = i * h;
    }

    // Allocate global solution vector
    std::vector<double> T(num_nodes, 0.0);
    T[0] = config.temp_left_C;
    T[N] = config.temp_right_C;

    if (N == 1) {
        // Only boundary nodes exist
    } else {
        // Interior nodes: 1 to N-1 (size = N - 1)
        const int m = N - 1;
        // Tridiagonal system: a[i] * T_{i-1} + b[i] * T_i + c[i] * T_{i+1} = d[i]
        // For standard 1D linear FEA with uniform mesh:
        // Local K^e = (kA/h) * [ 1, -1; -1, 1 ]
        // Local F^e = (qAh/2) * [ 1; 1 ]
        const double diag = 2.0 * k * A / h;
        const double off_diag = -k * A / h;
        const double interior_load = q * A * h; // Sum of load contributions from both adjacent elements

        std::vector<double> a(m, off_diag); // Subdiagonal
        std::vector<double> b(m, diag);     // Main diagonal
        std::vector<double> c(m, off_diag); // Superdiagonal
        std::vector<double> d(m, interior_load); // Right-hand side vector

        // Impose Dirichlet boundary conditions on the RHS
        // Node 1 receives contribution from T[0]
        d[0] -= off_diag * T[0];
        // Node N-1 receives contribution from T[N]
        d[m - 1] -= off_diag * T[N];

        // Thomas Algorithm for Tridiagonal System (O(m) time and memory)
        // Forward elimination
        for (int i = 1; i < m; ++i) {
            double factor = a[i] / b[i - 1];
            b[i] -= factor * c[i - 1];
            d[i] -= factor * d[i - 1];
        }

        // Back substitution
        std::vector<double> T_interior(m);
        T_interior[m - 1] = d[m - 1] / b[m - 1];
        for (int i = m - 2; i >= 0; --i) {
            T_interior[i] = (d[i] - c[i] * T_interior[i + 1]) / b[i];
        }

        // Store into global solution vector
        for (int i = 0; i < m; ++i) {
            T[i + 1] = T_interior[i];
        }
    }

    // Exact solution comparison and midpoint error evaluation
    std::vector<double> T_exact(num_nodes);
    for (int i = 0; i < num_nodes; ++i) {
        T_exact[i] = exact_temperature(x[i], config);
    }

    // Evaluate midpoint error using FE linear shape function interpolation:
    // T_FE(x_mid) = 0.5 * (T_i + T_{i+1})
    std::vector<MidpointErrorSample> midpoint_errors(N);
    double max_err = 0.0;

    for (int e = 0; e < N; ++e) {
        double x_mid = (x[e] + x[e + 1]) * 0.5;
        double T_fe_mid = (T[e] + T[e + 1]) * 0.5;
        double T_exact_mid = exact_temperature(x_mid, config);
        double err = std::abs(T_fe_mid - T_exact_mid);

        midpoint_errors[e] = MidpointErrorSample{
            e,
            x_mid,
            T_fe_mid,
            T_exact_mid,
            err
        };

        if (err > max_err) {
            max_err = err;
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double duration_us = std::chrono::duration<double, std::micro>(end_time - start_time).count();

    FeaSolution solution;
    solution.config = config;
    solution.x_coords = std::move(x);
    solution.temperatures_fe = std::move(T);
    solution.temperatures_exact = std::move(T_exact);
    solution.midpoint_errors = std::move(midpoint_errors);
    solution.max_absolute_midpoint_error_C = max_err;
    solution.solve_duration_us = duration_us;

    return core::Result<FeaSolution>::Ok(solution);
}

std::vector<ConvergenceRecord> ThermalFeaSolver::run_convergence_study(const ThermalProblemConfig& base_config) {
    const std::vector<int> element_counts = { 10, 20, 40, 80 };
    std::vector<ConvergenceRecord> records;
    records.reserve(element_counts.size());

    double prev_error = 0.0;

    for (int N : element_counts) {
        ThermalProblemConfig cfg = base_config;
        cfg.element_count = N;
        auto result = solve(cfg);
        if (result.success && result.data) {
            double h = cfg.length_m / static_cast<double>(N);
            double err = result.data->max_absolute_midpoint_error_C;
            double rate = 0.0;
            if (prev_error > 0.0 && err > 0.0) {
                rate = std::log2(prev_error / err);
            }
            records.push_back(ConvergenceRecord{ N, h, err, rate });
            prev_error = err;
        }
    }

    return records;
}

std::string ThermalFeaSolver::export_to_csv(const FeaSolution& solution) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(8);
    ss << "# Unified Engineering Workbench - 1D Thermal FEA Solution\n";
    ss << "# L=" << solution.config.length_m << "m, A=" << solution.config.area_m2 << "m2, k=" 
       << solution.config.conductivity_k << "W/(m K), q=" << solution.config.heat_gen_q << "W/m3\n";
    ss << "# T(0)=" << solution.config.temp_left_C << "C, T(L)=" << solution.config.temp_right_C << "C\n";
    ss << "type,index,x_m,T_FE_C,T_exact_C,absolute_error_C\n";

    // Export nodal values
    for (size_t i = 0; i < solution.x_coords.size(); ++i) {
        double err = std::abs(solution.temperatures_fe[i] - solution.temperatures_exact[i]);
        ss << "node," << i << "," << solution.x_coords[i] << "," << solution.temperatures_fe[i] << ","
           << solution.temperatures_exact[i] << "," << err << "\n";
    }

    // Export midpoint values
    for (const auto& mp : solution.midpoint_errors) {
        ss << "midpoint," << mp.element_index << "," << mp.x_mid_m << "," << mp.temp_fe_C << ","
           << mp.temp_exact_C << "," << mp.absolute_error_C << "\n";
    }

    return ss.str();
}

std::string ThermalFeaSolver::export_convergence_to_csv(const std::vector<ConvergenceRecord>& records) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(8);
    ss << "elements_N,mesh_size_h_m,max_midpoint_error_C,convergence_rate_order\n";
    for (const auto& r : records) {
        ss << r.elements << "," << r.mesh_size_h << "," << r.max_midpoint_error_C << "," << r.convergence_rate << "\n";
    }
    return ss.str();
}

} // namespace workbench::fea

