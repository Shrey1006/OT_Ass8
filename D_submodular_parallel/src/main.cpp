#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <numeric>
#include <cmath>
#include "parallel_workload.hpp"

int main() {
    std::cout << "=========================================================================================\n";
    std::cout << "  Track D2: Parallel Workload Benchmark (OpenMP vs std::thread vs Work-Stealing)        \n";
    std::cout << "=========================================================================================\n\n";

    const size_t total_paths = 2000000;
    const size_t num_chunks = 64;
    const unsigned int base_seed = 42;
    const std::vector<size_t> thread_counts = {1, 2, 4, 8};
    const size_t num_runs = 10;

    std::string csv_scaling = "D_submodular_parallel/results/parallel_scaling_benchmark.csv";
    std::ofstream csv(csv_scaling);
    if (!csv.is_open()) {
        csv_scaling = "results/parallel_scaling_benchmark.csv";
        csv.open(csv_scaling);
    }
    if (!csv.is_open()) {
        std::cerr << "[ERROR] Could not open output CSV file." << std::endl;
        return 1;
    }

    csv << "framework,schedule,threads,mean_runtime_ms,std_runtime_ms,speedup,efficiency,amdahl_speedup,gustafson_speedup\n";

    // Measure baseline serial runtime
    std::vector<double> serial_times;
    double serial_val = 0.0;
    for (size_t r = 0; r < num_runs; ++r) {
        auto t0 = std::chrono::steady_clock::now();
        serial_val = opt::ParallelBenchmark::run_serial(total_paths, num_chunks, base_seed + static_cast<unsigned int>(r));
        auto t1 = std::chrono::steady_clock::now();
        serial_times.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    double t_serial_mean = std::accumulate(serial_times.begin(), serial_times.end(), 0.0) / num_runs;
    std::cout << "[BASELINE] Serial Monte Carlo Runtime (1 Thread): " << std::fixed << std::setprecision(2)
              << t_serial_mean << " ms | Option Price = " << serial_val << "\n\n";

    // Estimate serial fraction s for Amdahl & Gustafson (typically ~2% for memory reduction and initialization)
    const double s_fraction = 0.035;

    struct MethodConfig {
        std::string framework;
        std::string schedule;
        std::function<double(size_t, size_t, size_t, unsigned int)> fn;
    };

    std::vector<MethodConfig> methods = {
        {"OpenMP", "Static", [](size_t p, size_t c, size_t th, unsigned int s) {
            return opt::ParallelBenchmark::run_openmp_static(p, c, th, s);
        }},
        {"OpenMP", "Dynamic", [](size_t p, size_t c, size_t th, unsigned int s) {
            return opt::ParallelBenchmark::run_openmp_dynamic(p, c, th, s);
        }},
        {"std::thread", "Static", [](size_t p, size_t c, size_t th, unsigned int s) {
            return opt::ParallelBenchmark::run_std_thread_static(p, c, th, s);
        }},
        {"std::thread", "Dynamic", [](size_t p, size_t c, size_t th, unsigned int s) {
            return opt::ParallelBenchmark::run_std_thread_dynamic(p, c, th, s);
        }},
        {"std::thread", "Work-Stealing", [](size_t p, size_t c, size_t th, unsigned int s) {
            return opt::ParallelBenchmark::run_std_thread_work_stealing(p, c, th, s);
        }}
    };

    std::cout << std::left
              << std::setw(14) << "Framework"
              << std::setw(16) << "Schedule"
              << std::setw(10) << "Threads"
              << std::setw(18) << "Runtime (ms)"
              << std::setw(10) << "Speedup"
              << std::setw(12) << "Efficiency"
              << std::setw(12) << "Amdahl S"
              << std::setw(14) << "Gustafson S"
              << "\n";
    std::cout << std::string(104, '-') << "\n";

    for (const auto& m : methods) {
        for (size_t th : thread_counts) {
            std::vector<double> runtimes;
            double computed_val = 0.0;

            for (size_t r = 0; r < num_runs; ++r) {
                unsigned int seed = base_seed + static_cast<unsigned int>(r * 10);
                auto t0 = std::chrono::steady_clock::now();
                computed_val = m.fn(total_paths, num_chunks, th, seed);
                auto t1 = std::chrono::steady_clock::now();
                runtimes.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
            }

            double mean_t = std::accumulate(runtimes.begin(), runtimes.end(), 0.0) / num_runs;
            double sq_sum = 0.0;
            for (double t : runtimes) sq_sum += (t - mean_t) * (t - mean_t);
            double std_t = std::sqrt(sq_sum / (num_runs - 1));

            double speedup = t_serial_mean / mean_t;
            double efficiency = speedup / static_cast<double>(th);

            // Theoretical predictions
            double amdahl_s = 1.0 / (s_fraction + (1.0 - s_fraction) / static_cast<double>(th));
            double gustafson_s = static_cast<double>(th) - s_fraction * static_cast<double>(th - 1);

            std::string time_str = std::to_string(mean_t).substr(0, 5) + " +/- " + std::to_string(std_t).substr(0, 4);

            std::cout << std::left
                      << std::setw(14) << m.framework
                      << std::setw(16) << m.schedule
                      << std::setw(10) << th
                      << std::setw(18) << time_str
                      << std::setw(10) << std::fixed << std::setprecision(2) << speedup
                      << std::setw(12) << std::fixed << std::setprecision(2) << efficiency
                      << std::setw(12) << std::fixed << std::setprecision(2) << amdahl_s
                      << std::setw(14) << std::fixed << std::setprecision(2) << gustafson_s
                      << "\n";

            csv << m.framework << ","
                << m.schedule << ","
                << th << ","
                << mean_t << ","
                << std_t << ","
                << speedup << ","
                << efficiency << ","
                << amdahl_s << ","
                << gustafson_s << "\n";
        }
        std::cout << std::string(104, '-') << "\n";
    }
    csv.close();

    // ==============================================================================
    // Experiment 2: Where Overhead Breaks Scaling (Granularity Breakdown)
    // ==============================================================================
    std::cout << "\n=========================================================================================\n";
    std::cout << "  Experiment 2: Where Overhead Breaks Scaling (Fine- vs. Coarse-Grained Task Granularity)\n";
    std::cout << "=========================================================================================\n";

    std::string csv_overhead = "D_submodular_parallel/results/overhead_breakdown.csv";
    std::ofstream csv_ov(csv_overhead);
    if (!csv_ov.is_open()) csv_ov.open("results/overhead_breakdown.csv");

    csv_ov << "granularity,paths_per_task,num_tasks,threads,runtime_ms,speedup,efficiency\n";

    struct GranularityTest {
        std::string label;
        size_t paths_per_task;
        size_t num_tasks;
    };

    std::vector<GranularityTest> gran_tests = {
        {"Coarse-Grained (High Arithmetic Intensity)", 50000, 40},
        {"Medium-Grained", 2500, 800},
        {"Fine-Grained (Severe Synchronization Overhead)", 50, 40000}
    };

    std::cout << std::left
              << std::setw(46) << "Workload Granularity"
              << std::setw(10) << "Threads"
              << std::setw(16) << "Runtime (ms)"
              << std::setw(10) << "Speedup"
              << std::setw(12) << "Efficiency"
              << "\n";
    std::cout << std::string(94, '-') << "\n";

    for (const auto& gt : gran_tests) {
        size_t total_p = gt.paths_per_task * gt.num_tasks;
        auto t0_s = std::chrono::steady_clock::now();
        opt::ParallelBenchmark::run_serial(total_p, gt.num_tasks, base_seed);
        auto t1_s = std::chrono::steady_clock::now();
        double t_base = std::chrono::duration<double, std::milli>(t1_s - t0_s).count();

        for (size_t th : {1, 4, 8}) {
            auto t0 = std::chrono::steady_clock::now();
            opt::ParallelBenchmark::run_std_thread_dynamic(total_p, gt.num_tasks, th, base_seed);
            auto t1 = std::chrono::steady_clock::now();
            double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            double sp = t_base / ms;
            double eff = sp / static_cast<double>(th);

            std::cout << std::left
                      << std::setw(46) << gt.label
                      << std::setw(10) << th
                      << std::setw(16) << std::fixed << std::setprecision(2) << ms
                      << std::setw(10) << std::fixed << std::setprecision(2) << sp
                      << std::setw(12) << std::fixed << std::setprecision(2) << eff
                      << "\n";

            csv_ov << "\"" << gt.label << "\","
                   << gt.paths_per_task << ","
                   << gt.num_tasks << ","
                   << th << ","
                   << ms << ","
                   << sp << ","
                   << eff << "\n";
        }
        std::cout << std::string(94, '-') << "\n";
    }
    csv_ov.close();

    // ==============================================================================
    // Experiment 3: Demonstrations of Data, Model, and Pipeline Parallelism
    // ==============================================================================
    std::cout << "\n=========================================================================================\n";
    std::cout << "  Experiment 3: Data, Model, and Pipeline Parallelism Demonstrations                      \n";
    std::cout << "=========================================================================================\n";

    // 1. Data Parallelism Demo
    std::vector<double> input_data(100000);
    for (size_t i = 0; i < input_data.size(); ++i) input_data[i] = static_cast<double>(i) * 0.001;
    auto dp_out = opt::ParallelParadigmsDemo::run_data_parallelism(input_data, 4);
    std::cout << "[DEMO 1: DATA PARALLELISM] Processed " << dp_out.size()
              << " elements across 4 data-partitioned threads. Sample y[100] = " << dp_out[100] << "\n";

    // 2. Model Parallelism Demo
    std::vector<double> model_in(1000, 0.5);
    auto mp_out = opt::ParallelParadigmsDemo::run_model_parallelism(model_in);
    std::cout << "[DEMO 2: MODEL PARALLELISM] Evaluated 2-layer neural operator split across Thread 1 (Layer 1) and Thread 2 (Layer 2). Sample out[0] = "
              << mp_out[0] << "\n";

    // 3. Pipeline Parallelism Demo
    size_t pipe_items = 50000;
    size_t processed = opt::ParallelParadigmsDemo::run_pipeline_parallelism(pipe_items);
    std::cout << "[DEMO 3: PIPELINE PARALLELISM] Streamed " << processed
              << " items through 3 concurrent stages (Ingestion -> Transform -> Aggregation) via thread-safe queues.\n\n";

    std::cout << "[SUCCESS] Parallel workload benchmark complete.\n";
    std::cout << "Results exported to: " << csv_scaling << " and " << csv_overhead << "\n";
    return 0;
}
