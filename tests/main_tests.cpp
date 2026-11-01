#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <functional>
#include <chrono>

#include "parser/stl_parser.hpp"
#include "parser/ansys_parser.hpp"
#include "fea/fea_solver.hpp"
#include "multicore/batch_solver.hpp"
#include "embedded/embedded_mock.hpp"
#include "backprop/backpropagation.hpp"

using namespace workbench;

struct TestCase {
    std::string suite;
    std::string name;
    std::function<bool(std::string& failure_msg)> run;
};

int main() {
    std::vector<TestCase> tests;

    // --- Task B: Parser Tests ---
    tests.push_back({ "Task B - CAD/ANSYS Parser", "Valid ASCII STL Parse", [](std::string& msg) {
        auto res = parser::StlParser::parse_file("data/bracket.stl");
        if (!res.success || !res.data) {
            msg = "Failed to parse data/bracket.stl: " + res.error_message;
            return false;
        }
        if (res.data->facets.size() != 20) {
            msg = "Expected 20 facets in bracket.stl, got " + std::to_string(res.data->facets.size());
            return false;
        }
        return true;
    }});

    tests.push_back({ "Task B - CAD/ANSYS Parser", "Binary STL Rejection", [](std::string& msg) {
        std::string binary_stl(100, '\0');
        auto res = parser::StlParser::parse_string(binary_stl);
        if (res.success) {
            msg = "Expected parser to reject binary STL, but it succeeded.";
            return false;
        }
        return true;
    }});

    tests.push_back({ "Task B - CAD/ANSYS Parser", "Empty STL Explanation", [](std::string& msg) {
        auto res = parser::StlParser::parse_string("");
        if (res.success) {
            msg = "Expected parser to fail on empty string.";
            return false;
        }
        return true;
    }});

    tests.push_back({ "Task B - CAD/ANSYS Parser", "Valid ANSYS CSV Parse", [](std::string& msg) {
        auto res = parser::AnsysParser::parse_file("data/synthetic_ansys_results.csv");
        if (!res.success || !res.data) {
            msg = "Failed to parse data/synthetic_ansys_results.csv: " + res.error_message;
            return false;
        }
        if (res.data->nodes.size() != 264) {
            msg = "Expected 264 nodes, got " + std::to_string(res.data->nodes.size());
            return false;
        }
        if (res.data->min_temp_C < 15.0 || res.data->max_temp_C > 120.0) {
            msg = "Unexpected temperature range: [" + std::to_string(res.data->min_temp_C) + ", " + std::to_string(res.data->max_temp_C) + "]";
            return false;
        }
        return true;
    }});

    tests.push_back({ "Task B - CAD/ANSYS Parser", "ANSYS CSV Malformed Row Rejection", [](std::string& msg) {
        std::string bad_csv = "node_id,x_m,y_m,z_m,temperature_C\n1,0.0,0.0,0.0,NaN\n";
        auto res = parser::AnsysParser::parse_string(bad_csv);
        if (res.success) {
            msg = "Expected rejection of non-finite NaN temperature, but parser succeeded.";
            return false;
        }
        return true;
    }});

    // --- Task C: Thermal FEA Tests ---
    tests.push_back({ "Task C - Thermal FEA", "Dirichlet Boundary Imposition", [](std::string& msg) {
        fea::ThermalProblemConfig cfg;
        cfg.element_count = 10;
        cfg.temp_left_C = 100.0;
        cfg.temp_right_C = 20.0;
        auto res = fea::ThermalFeaSolver::solve(cfg);
        if (!res.success || !res.data) {
            msg = "FEA solve failed: " + res.error_message;
            return false;
        }
        double t0 = res.data->temperatures_fe.front();
        double tL = res.data->temperatures_fe.back();
        if (std::abs(t0 - 100.0) > 1e-12 || std::abs(tL - 20.0) > 1e-12) {
            msg = "Boundary conditions violated: T(0)=" + std::to_string(t0) + ", T(L)=" + std::to_string(tL);
            return false;
        }
        return true;
    }});

    tests.push_back({ "Task C - Thermal FEA", "Nodal Exact Agreement", [](std::string& msg) {
        // In 1D with piecewise constant heat generation, 2-node linear elements have zero nodal error
        fea::ThermalProblemConfig cfg;
        cfg.element_count = 20;
        auto res = fea::ThermalFeaSolver::solve(cfg);
        if (!res.success || !res.data) return false;
        for (size_t i = 0; i < res.data->temperatures_fe.size(); ++i) {
            double diff = std::abs(res.data->temperatures_fe[i] - res.data->temperatures_exact[i]);
            if (diff > 1e-10) {
                msg = "Nodal error > 1e-10 at node " + std::to_string(i) + ": diff=" + std::to_string(diff);
                return false;
            }
        }
        return true;
    }});

    tests.push_back({ "Task C - Thermal FEA", "Midpoint Mesh Convergence O(h^2)", [](std::string& msg) {
        auto conv = fea::ThermalFeaSolver::run_convergence_study();
        if (conv.size() != 4) {
            msg = "Expected 4 convergence entries for N=10, 20, 40, 80";
            return false;
        }
        // Check theoretical midpoint errors:
        // E = q*h^2 / (8*k) = 125 * h^2
        // For N=10: h=0.1 -> 1.25 C
        // For N=20: h=0.05 -> 0.3125 C (ratio 4.0, rate ~2.0)
        // For N=40: h=0.025 -> 0.078125 C (ratio 4.0, rate ~2.0)
        // For N=80: h=0.0125 -> 0.01953125 C (ratio 4.0, rate ~2.0)
        if (std::abs(conv[0].max_midpoint_error_C - 1.25) > 1e-4) {
            msg = "N=10 midpoint error mismatch: got " + std::to_string(conv[0].max_midpoint_error_C);
            return false;
        }
        if (std::abs(conv[1].max_midpoint_error_C - 0.3125) > 1e-4) {
            msg = "N=20 midpoint error mismatch: got " + std::to_string(conv[1].max_midpoint_error_C);
            return false;
        }
        for (size_t i = 1; i < conv.size(); ++i) {
            if (std::abs(conv[i].convergence_rate - 2.0) > 0.05) {
                msg = "Convergence rate at step " + std::to_string(i) + " deviates from 2.0: got " + std::to_string(conv[i].convergence_rate);
                return false;
            }
        }
        return true;
    }});

    // --- Task D: Multicore Tests ---
    tests.push_back({ "Task D - Multicore Batch", "Parallel-Serial Agreement <= 1e-8 C", [](std::string& msg) {
        auto batch = multicore::MulticoreBatchSolver::generate_batch(50, 40, 1337);
        auto serial = multicore::MulticoreBatchSolver::solve_serial(batch);
        auto parallel = multicore::MulticoreBatchSolver::solve_parallel(batch, 4);
        double max_diff = multicore::MulticoreBatchSolver::verify_agreement(serial, parallel);
        if (max_diff > 1.0e-8) {
            msg = "Parallel/Serial discrepancy exceeded 1e-8 C: " + std::to_string(max_diff);
            return false;
        }
        return true;
    }});

    // --- Task E: Embedded Integration Tests ---
    tests.push_back({ "Task E - Embedded Mock", "Hardware Discovery (MCU, FPGA, JTAG)", [](std::string& msg) {
        embedded::EmbeddedWorkbenchManager mgr;
        auto devices = mgr.discover_devices();
        if (devices.size() != 3) {
            msg = "Expected 3 devices, found: " + std::to_string(devices.size());
            return false;
        }
        return true;
    }});

    tests.push_back({ "Task E - Embedded Mock", "Fault Injection & Safe Recovery", [](std::string& msg) {
        embedded::EmbeddedWorkbenchManager mgr;
        mgr.connect_all();

        auto mcu = mgr.get_adapter("MCU-STM32-01");
        if (!mcu) { msg = "MCU adapter not found"; return false; }

        // Normal read
        auto normal_read = mcu->read_telemetry();
        if (!normal_read.success) { msg = "Normal read failed"; return false; }

        // Inject timeout fault
        mcu->inject_fault(true);
        auto fault_read = mcu->read_telemetry();
        if (fault_read.success) {
            msg = "Expected fault read to fail with timeout.";
            return false;
        }

        // Clear fault & recover
        mcu->inject_fault(false);
        auto recovered_read = mcu->read_telemetry();
        if (!recovered_read.success) {
            msg = "Failed to recover after clearing fault: " + recovered_read.error_message;
            return false;
        }
        return true;
    }});

    // --- Task F: Backpropagation Tests ---
    tests.push_back({ "Task F - Backpropagation", "Initial Forward Pass Numerical Values", [](std::string& msg) {
        backprop::NetworkParameters params{ 0.5, 0.0, 1.0, 0.0 };
        auto fwd = backprop::BackpropEngine::forward(params, 1.0, 2.0);
        // z = 0.5
        // h = tanh(0.5) ~ 0.4621171573
        // y_hat = 0.4621171573
        // J = 0.5 * (0.4621171573 - 2)^2 = 0.5 * (-1.5378828)^2 ~ 1.1825418
        if (std::abs(fwd.z - 0.5) > 1e-6) { msg = "z mismatch"; return false; }
        if (std::abs(fwd.h - 0.462117) > 1e-4) { msg = "h mismatch"; return false; }
        if (std::abs(fwd.loss - 1.18254) > 1e-4) { msg = "loss mismatch: " + std::to_string(fwd.loss); return false; }
        return true;
    }});

    tests.push_back({ "Task F - Backpropagation", "Central Finite Difference Grad Check (eps=1e-6, err<=1e-5)", [](std::string& msg) {
        auto report = backprop::BackpropEngine::verify_gradients_finite_difference();
        if (!report.all_passed) {
            std::ostringstream ss;
            ss << "Gradient check failed:\n";
            for (const auto& e : report.entries) {
                ss << "  " << e.parameter_name << ": analytical=" << e.analytical_grad 
                   << ", numerical=" << e.numerical_grad << ", error=" << e.absolute_error 
                   << (e.passed ? " [OK]" : " [FAIL]") << "\n";
            }
            msg = ss.str();
            return false;
        }
        return true;
    }});

    tests.push_back({ "Task F - Backpropagation", "100-Step Training Loss Monotonic Convergence", [](std::string& msg) {
        auto history = backprop::BackpropEngine::train_100_steps();
        if (history.size() != 101) {
            msg = "Expected 101 records (0 to 100), got " + std::to_string(history.size());
            return false;
        }
        double initial_loss = history.front().forward.loss;
        double final_loss = history.back().forward.loss;
        if (final_loss >= initial_loss) {
            msg = "Loss did not decrease: J0=" + std::to_string(initial_loss) + ", J100=" + std::to_string(final_loss);
            return false;
        }
        // Loss should reduce significantly (at least 90% reduction)
        if (final_loss > initial_loss * 0.1) {
            msg = "Convergence too slow: final loss=" + std::to_string(final_loss);
            return false;
        }
        return true;
    }});

    // --- Execution Runner ---
    std::cout << "\n=======================================================\n";
    std::cout << "  UNIFIED ENGINEERING WORKBENCH - VERIFICATION SUITE   \n";
    std::cout << "=======================================================\n\n";

    int passed = 0;
    int failed = 0;

    for (size_t i = 0; i < tests.size(); ++i) {
        const auto& t = tests[i];
        std::cout << "[" << std::setw(2) << (i + 1) << "/" << tests.size() << "] " 
                  << std::left << std::setw(32) << t.suite << " | "
                  << std::setw(42) << t.name << " : ";

        std::string err_msg;
        auto start = std::chrono::high_resolution_clock::now();
        bool ok = t.run(err_msg);
        auto end = std::chrono::high_resolution_clock::now();
        double elapsed_us = std::chrono::duration<double, std::micro>(end - start).count();

        if (ok) {
            passed++;
            std::cout << "PASS (" << std::fixed << std::setprecision(1) << elapsed_us << " us)\n";
        } else {
            failed++;
            std::cout << "FAIL!\n      Error: " << err_msg << "\n";
        }
    }

    std::cout << "\n-------------------------------------------------------\n";
    std::cout << "Summary: " << passed << " passed, " << failed << " failed out of " 
              << tests.size() << " tests.\n";
    std::cout << "=======================================================\n\n";

    return (failed == 0) ? 0 : 1;
}
