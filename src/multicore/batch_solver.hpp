#pragma once

#include "fea/fea_solver.hpp"
#include <vector>
#include <string>
#include <cstdint>

namespace workbench::multicore {

/**
 * System hardware specification telemetry.
 */
struct SystemHardwareInfo {
    std::string cpu_model{};
    uint32_t logical_cores{1};
    uint32_t physical_cores{1};
    uint64_t total_ram_mb{0};
    std::string compiler_info{};
    std::string optimization_flags{};
};

/**
 * Batch execution benchmark result for a specific worker thread count.
 */
struct ThreadBenchmarkResult {
    int worker_count{1};
    std::vector<double> repetition_times_ms;
    double median_time_ms{0.0};
    double speedup{1.0};
    double efficiency{1.0};
    double max_absolute_diff_vs_serial{0.0};
    bool verified_agreement{false}; // diff <= 1e-8 C
};

/**
 * Complete multicore benchmark suite result.
 */
struct BatchBenchmarkSuiteResult {
    SystemHardwareInfo hardware_info;
    int case_count{0};
    int elements_per_case{0};
    uint64_t random_seed{0};
    double launch_to_ready_ms{0.0};
    std::vector<ThreadBenchmarkResult> thread_results;
    std::string analysis_notes{};
};

/**
 * Multicore batch solver and benchmark engine.
 */
class MulticoreBatchSolver {
public:
    /**
     * Generates a deterministic, reproducible batch of thermal cases.
     */
    static std::vector<fea::ThermalProblemConfig> generate_batch(
        int case_count, 
        int element_count = 100, 
        uint64_t seed = 1337
    );

    /**
     * Solves a batch serially on a single thread.
     */
    static std::vector<fea::FeaSolution> solve_serial(
        const std::vector<fea::ThermalProblemConfig>& batch
    );

    /**
     * Solves a batch in parallel using a bounded worker pool of p threads.
     * Guarantees deterministic input ordering.
     */
    static std::vector<fea::FeaSolution> solve_parallel(
        const std::vector<fea::ThermalProblemConfig>& batch, 
        int num_workers
    );

    /**
     * Compares serial and parallel results and returns maximum absolute difference across all nodes.
     */
    static double verify_agreement(
        const std::vector<fea::FeaSolution>& serial_results,
        const std::vector<fea::FeaSolution>& parallel_results
    );

    /**
     * Gathers local hardware and environment telemetry.
     */
    static SystemHardwareInfo detect_hardware();

    /**
     * Runs the full 5-repetition benchmark suite for 1, 2, and 4 workers.
     */
    static BatchBenchmarkSuiteResult run_full_benchmark(
        int case_count = 1000, 
        int element_count = 100, 
        uint64_t seed = 1337
    );

    /**
     * Exports benchmark suite results to CSV format.
     */
    static std::string export_benchmark_to_csv(const BatchBenchmarkSuiteResult& suite);
};

} // namespace workbench::multicore

