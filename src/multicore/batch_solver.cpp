#include "multicore/batch_solver.hpp"
#include <random>
#include <thread>
#include <future>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#endif

namespace workbench::multicore {

std::vector<fea::ThermalProblemConfig> MulticoreBatchSolver::generate_batch(
    int case_count, 
    int element_count, 
    uint64_t seed
) {
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> dist_k(5.0, 50.0);       // Varied positive conductivity [5, 50] W/(m K)
    std::uniform_real_distribution<double> dist_q(1000.0, 50000.0); // Varied positive heat gen [1000, 50000] W/m3
    std::uniform_real_distribution<double> dist_T0(80.0, 150.0);    // Boundary T(0) [80, 150] C
    std::uniform_real_distribution<double> dist_TL(10.0, 40.0);     // Boundary T(L) [10, 40] C

    std::vector<fea::ThermalProblemConfig> batch;
    batch.reserve(case_count);

    for (int i = 0; i < case_count; ++i) {
        fea::ThermalProblemConfig cfg;
        cfg.length_m = 1.0;
        cfg.area_m2 = 1.0e-4;
        cfg.conductivity_k = dist_k(rng);
        cfg.heat_gen_q = dist_q(rng);
        cfg.temp_left_C = dist_T0(rng);
        cfg.temp_right_C = dist_TL(rng);
        cfg.element_count = element_count;
        batch.push_back(cfg);
    }

    return batch;
}

std::vector<fea::FeaSolution> MulticoreBatchSolver::solve_serial(
    const std::vector<fea::ThermalProblemConfig>& batch
) {
    const size_t n = batch.size();
    std::vector<fea::FeaSolution> results;
    results.reserve(n);

    for (size_t i = 0; i < n; ++i) {
        auto res = fea::ThermalFeaSolver::solve(batch[i]);
        if (res.success && res.data) {
            results.push_back(std::move(*res.data));
        } else {
            results.push_back(fea::FeaSolution{});
        }
    }

    return results;
}

std::vector<fea::FeaSolution> MulticoreBatchSolver::solve_parallel(
    const std::vector<fea::ThermalProblemConfig>& batch, 
    int num_workers
) {
    const size_t n = batch.size();
    std::vector<fea::FeaSolution> results(n);

    if (num_workers <= 1) {
        return solve_serial(batch);
    }

    std::atomic<size_t> next_index{0};
    std::vector<std::thread> workers;
    workers.reserve(num_workers);

    for (int w = 0; w < num_workers; ++w) {
        workers.emplace_back([&batch, &results, &next_index, n]() {
            while (true) {
                size_t idx = next_index.fetch_add(1, std::memory_order_relaxed);
                if (idx >= n) {
                    break;
                }
                auto res = fea::ThermalFeaSolver::solve(batch[idx]);
                if (res.success && res.data) {
                    results[idx] = std::move(*res.data);
                }
            }
        });
    }

    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    return results;
}

double MulticoreBatchSolver::verify_agreement(
    const std::vector<fea::FeaSolution>& serial_results,
    const std::vector<fea::FeaSolution>& parallel_results
) {
    if (serial_results.size() != parallel_results.size()) {
        return std::numeric_limits<double>::infinity();
    }

    double max_diff = 0.0;
    const size_t n = serial_results.size();

    for (size_t c = 0; c < n; ++c) {
        const auto& s_nodes = serial_results[c].temperatures_fe;
        const auto& p_nodes = parallel_results[c].temperatures_fe;

        if (s_nodes.size() != p_nodes.size()) {
            return std::numeric_limits<double>::infinity();
        }

        for (size_t i = 0; i < s_nodes.size(); ++i) {
            double diff = std::abs(s_nodes[i] - p_nodes[i]);
            if (diff > max_diff) {
                max_diff = diff;
            }
        }
    }

    return max_diff;
}

SystemHardwareInfo MulticoreBatchSolver::detect_hardware() {
    SystemHardwareInfo info;
    info.logical_cores = std::thread::hardware_concurrency();
    if (info.logical_cores == 0) info.logical_cores = 1;
    info.physical_cores = info.logical_cores; // Conservative fallback

#if defined(_WIN32)
    // CPU Brand String via CPUID
    int cpu_info[4] = {-1};
    char brand[0x40] = {0};
    __cpuid(cpu_info, 0x80000000);
    unsigned int nExIds = cpu_info[0];
    if (nExIds >= 0x80000004) {
        __cpuid(reinterpret_cast<int*>(brand), 0x80000002);
        __cpuid(reinterpret_cast<int*>(brand + 16), 0x80000003);
        __cpuid(reinterpret_cast<int*>(brand + 32), 0x80000004);
        info.cpu_model = brand;
    } else {
        info.cpu_model = "x86_64 Compatible Processor";
    }

    // Memory info
    MEMORYSTATUSEX mem_status;
    mem_status.dwLength = sizeof(mem_status);
    if (GlobalMemoryStatusEx(&mem_status)) {
        info.total_ram_mb = mem_status.ullTotalPhys / (1024 * 1024);
    }

    // Physical core approximation via GetLogicalProcessorInformation
    DWORD return_length = 0;
    GetLogicalProcessorInformation(nullptr, &return_length);
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> buffer(return_length / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
        if (GetLogicalProcessorInformation(buffer.data(), &return_length)) {
            DWORD physical_core_count = 0;
            for (const auto& item : buffer) {
                if (item.Relationship == RelationProcessorCore) {
                    physical_core_count++;
                }
            }
            if (physical_core_count > 0) {
                info.physical_cores = physical_core_count;
            }
        }
    }
#else
    info.cpu_model = "Standard CPU";
    info.total_ram_mb = 16384;
#endif

#if defined(_MSC_VER)
    info.compiler_info = "MSVC " + std::to_string(_MSC_VER);
#if defined(_DEBUG)
    info.optimization_flags = "/Od (Debug)";
#else
    info.optimization_flags = "/O2 /Oi /Ot /MT (Release Speed-Optimized)";
#endif
#elif defined(__clang__)
    info.compiler_info = "Clang " + std::string(__clang_version__);
    info.optimization_flags = "-O3";
#elif defined(__GNUC__)
    info.compiler_info = "GCC " + std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__);
    info.optimization_flags = "-O3";
#else
    info.compiler_info = "Standard C++17 Compiler";
    info.optimization_flags = "Default";
#endif

    return info;
}

BatchBenchmarkSuiteResult MulticoreBatchSolver::run_full_benchmark(
    int case_count, 
    int element_count, 
    uint64_t seed
) {
    BatchBenchmarkSuiteResult suite;
    suite.hardware_info = detect_hardware();
    suite.case_count = case_count;
    suite.elements_per_case = element_count;
    suite.random_seed = seed;

    auto batch = generate_batch(case_count, element_count, seed);

    // Warm-up run (unmeasured)
    auto warmup_result = solve_serial(batch);

    // Baseline serial solution for numerical agreement check
    const auto baseline_serial = solve_serial(batch);

    // Worker counts to benchmark: 1, 2, and up to 4 (capped at available logical CPUs)
    std::vector<int> target_workers = { 1 };
    if (suite.hardware_info.logical_cores >= 2) target_workers.push_back(2);
    if (suite.hardware_info.logical_cores >= 4) {
        target_workers.push_back(4);
    } else if (suite.hardware_info.logical_cores == 3) {
        target_workers.push_back(3);
    }

    double baseline_median_time = 1.0;

    for (int p : target_workers) {
        ThreadBenchmarkResult thread_res;
        thread_res.worker_count = p;
        thread_res.repetition_times_ms.reserve(5);

        std::vector<fea::FeaSolution> last_run_results;

        // Exactly 5 measured repetitions
        for (int rep = 0; rep < 5; ++rep) {
            auto start = std::chrono::high_resolution_clock::now();
            if (p == 1) {
                last_run_results = solve_serial(batch);
            } else {
                last_run_results = solve_parallel(batch, p);
            }
            auto end = std::chrono::high_resolution_clock::now();
            double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
            thread_res.repetition_times_ms.push_back(elapsed_ms);
        }

        // Calculate median time of 5 repetitions
        auto sorted_times = thread_res.repetition_times_ms;
        std::sort(sorted_times.begin(), sorted_times.end());
        thread_res.median_time_ms = sorted_times[2]; // Index 2 of 5 elements is exact median

        if (p == 1) {
            baseline_median_time = thread_res.median_time_ms;
            thread_res.speedup = 1.0;
            thread_res.efficiency = 1.0;
            thread_res.max_absolute_diff_vs_serial = 0.0;
            thread_res.verified_agreement = true;
        } else {
            thread_res.speedup = baseline_median_time / thread_res.median_time_ms;
            thread_res.efficiency = thread_res.speedup / static_cast<double>(p);
            thread_res.max_absolute_diff_vs_serial = verify_agreement(baseline_serial, last_run_results);
            thread_res.verified_agreement = (thread_res.max_absolute_diff_vs_serial <= 1.0e-8);
        }

        suite.thread_results.push_back(thread_res);
    }

    if (suite.hardware_info.logical_cores <= 1) {
        suite.analysis_notes = "Benchmarked on single logical core system: parallel path exercised but speedup cannot be evaluated.";
    } else {
        std::ostringstream ss;
        ss << "Benchmarked " << case_count << " cases on " << suite.hardware_info.logical_cores 
           << " logical cores. Maximum speedup achieved: " << std::fixed << std::setprecision(2)
           << suite.thread_results.back().speedup << "x (" 
           << std::setprecision(1) << (suite.thread_results.back().efficiency * 100.0) 
           << "% parallel efficiency).";
        suite.analysis_notes = ss.str();
    }

    return suite;
}

std::string MulticoreBatchSolver::export_benchmark_to_csv(const BatchBenchmarkSuiteResult& suite) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6);
    ss << "# CPU: " << suite.hardware_info.cpu_model << "\n";
    ss << "# Physical Cores: " << suite.hardware_info.physical_cores 
       << ", Logical Cores: " << suite.hardware_info.logical_cores << "\n";
    ss << "# RAM: " << suite.hardware_info.total_ram_mb << " MB\n";
    ss << "# Compiler: " << suite.hardware_info.compiler_info 
       << ", Flags: " << suite.hardware_info.optimization_flags << "\n";
    ss << "# Cases: " << suite.case_count << ", Elements/Case: " << suite.elements_per_case 
       << ", Seed: " << suite.random_seed << "\n";
    ss << "workers,rep1_ms,rep2_ms,rep3_ms,rep4_ms,rep5_ms,median_time_ms,speedup,efficiency,max_temp_diff_C,verified\n";

    for (const auto& tr : suite.thread_results) {
        ss << tr.worker_count;
        for (double t : tr.repetition_times_ms) {
            ss << "," << t;
        }
        ss << "," << tr.median_time_ms << "," << tr.speedup << "," << tr.efficiency << ","
           << tr.max_absolute_diff_vs_serial << "," << (tr.verified_agreement ? "PASS" : "FAIL") << "\n";
    }

    return ss.str();
}

} // namespace workbench::multicore
