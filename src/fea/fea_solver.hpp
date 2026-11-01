#pragma once

#include "core/types.hpp"
#include <vector>
#include <string>

namespace workbench::fea {

/**
 * Physical problem parameters for 1D thermal FEA rod.
 */
struct ThermalProblemConfig {
    double length_m{1.0};            // L = 1.0 m
    double area_m2{1.0e-4};          // A = 1e-4 m2
    double conductivity_k{10.0};     // k = 10 W/(m K)
    double heat_gen_q{10000.0};      // q = 10,000 W/m3
    double temp_left_C{100.0};       // T(0) = 100 C
    double temp_right_C{20.0};       // T(L) = 20 C
    int element_count{10};           // N uniform elements
};

/**
 * Midpoint error evaluation record.
 */
struct MidpointErrorSample {
    int element_index{0};
    double x_mid_m{0.0};
    double temp_fe_C{0.0};
    double temp_exact_C{0.0};
    double absolute_error_C{0.0};
};

/**
 * FEA solution result container.
 */
struct FeaSolution {
    ThermalProblemConfig config;
    std::vector<double> x_coords;
    std::vector<double> temperatures_fe;
    std::vector<double> temperatures_exact;
    std::vector<MidpointErrorSample> midpoint_errors;
    double max_absolute_midpoint_error_C{0.0};
    double solve_duration_us{0.0};
};

/**
 * Convergence record for multiple mesh discretizations.
 */
struct ConvergenceRecord {
    int elements{0};
    double mesh_size_h{0.0};
    double max_midpoint_error_C{0.0};
    double convergence_rate{0.0}; // Empirical order of convergence log2(E_prev / E_curr)
};

/**
 * 1D Steady-State Thermal Finite Element Solver.
 */
class ThermalFeaSolver {
public:
    /**
     * Solves the 1D steady-state heat equation for given parameters.
     */
    static core::Result<FeaSolution> solve(const ThermalProblemConfig& config);

    /**
     * Computes the exact analytical solution at position x:
     * T_exact(x) = 100 - 80*x/L + [q / (2*k)] * x * (L - x)
     */
    static double exact_temperature(double x, const ThermalProblemConfig& config);

    /**
     * Executes the standard mesh convergence study for N = 10, 20, 40, 80 elements.
     */
    static std::vector<ConvergenceRecord> run_convergence_study(const ThermalProblemConfig& base_config = {});

    /**
     * Exports FEA solution and midpoint comparison to CSV format.
     */
    static std::string export_to_csv(const FeaSolution& solution);

    /**
     * Exports mesh convergence table to CSV format.
     */
    static std::string export_convergence_to_csv(const std::vector<ConvergenceRecord>& records);
};

} // namespace workbench::fea

