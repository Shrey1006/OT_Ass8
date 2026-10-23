#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <cassert>
#include <chrono>
#include "knapsack.hpp"

int main() {
    std::cout << "====================================================================\n";
    std::cout << "   Track B: Discrete Optimization - 0/1 Knapsack Benchmark Suite   \n";
    std::cout << "====================================================================\n\n";

    const std::vector<size_t> sizes = {10, 20, 50, 100, 500};
    const unsigned int seed = 42;
    const double capacity_ratio = 0.5;

    // Output CSV file
    std::string csv_path = "B_discrete/results/knapsack_benchmark.csv";
    std::ofstream csv(csv_path);
    if (!csv.is_open()) {
        csv_path = "results/knapsack_benchmark.csv";
        csv.open(csv_path);
    }
    if (!csv.is_open()) {
        std::cerr << "[ERROR] Could not open output CSV file." << std::endl;
        return 1;
    }

    csv << "size,capacity,method,total_value,total_weight,exact_optimum,optimality_gap_percent,runtime_us,is_exact\n";

    std::cout << std::left
              << std::setw(6)  << "Size"
              << std::setw(38) << "Method"
              << std::setw(14) << "Value"
              << std::setw(14) << "Weight"
              << std::setw(12) << "Cap"
              << std::setw(14) << "Opt Gap (%)"
              << std::setw(16) << "Runtime (us)"
              << "\n";
    std::cout << std::string(114, '-') << "\n";

    for (size_t n : sizes) {
        auto inst = opt::KnapsackInstance::generate_random(n, capacity_ratio, seed);

        // 1. Exact reference: Dynamic Programming
        auto res_dp = opt::KnapsackSolver::solve_dynamic_programming(inst);
        double exact_opt = res_dp.total_value;

        // 2. Greedy by value/weight
        auto res_greedy = opt::KnapsackSolver::solve_greedy(inst);

        // 3. Integer Linear Programming (OR-Tools)
        auto res_ilp = opt::KnapsackSolver::solve_ilp(inst);

        // 4. Branch-and-Bound with Dantzig fractional bound
        auto res_bb = opt::KnapsackSolver::solve_branch_and_bound(inst);

        // Correctness verification across all exact methods
        if (std::abs(res_dp.total_value - res_bb.total_value) > 1e-4) {
            std::cerr << "[ERROR] DP and B&B value mismatch at size " << n 
                      << ": DP=" << res_dp.total_value << ", B&B=" << res_bb.total_value << std::endl;
            return 1;
        }
        if (std::abs(res_dp.total_value - res_ilp.total_value) > 1e-4) {
            std::cerr << "[ERROR] DP and ILP value mismatch at size " << n 
                      << ": DP=" << res_dp.total_value << ", ILP=" << res_ilp.total_value << std::endl;
            return 1;
        }

        // All weight constraints must be satisfied
        assert(res_greedy.total_weight <= inst.capacity);
        assert(res_dp.total_weight <= inst.capacity);
        assert(res_bb.total_weight <= inst.capacity);
        assert(res_ilp.total_weight <= inst.capacity);

        std::vector<opt::KnapsackResult> results = {res_greedy, res_dp, res_bb, res_ilp};

        for (const auto& r : results) {
            double gap_pct = 0.0;
            if (exact_opt > 0.0) {
                gap_pct = ((exact_opt - r.total_value) / exact_opt) * 100.0;
            }
            if (gap_pct < 0.0 && std::abs(gap_pct) < 1e-5) gap_pct = 0.0;

            std::cout << std::left
                      << std::setw(6)  << n
                      << std::setw(38) << r.method
                      << std::setw(14) << std::fixed << std::setprecision(1) << r.total_value
                      << std::setw(14) << std::fixed << std::setprecision(1) << r.total_weight
                      << std::setw(12) << std::fixed << std::setprecision(1) << inst.capacity
                      << std::setw(14) << std::fixed << std::setprecision(4) << gap_pct
                      << std::setw(16) << std::fixed << std::setprecision(2) << r.runtime_us
                      << "\n";

            csv << n << ","
                << inst.capacity << ","
                << "\"" << r.method << "\","
                << r.total_value << ","
                << r.total_weight << ","
                << exact_opt << ","
                << gap_pct << ","
                << r.runtime_us << ","
                << (r.is_exact ? "true" : "false") << "\n";
        }
        std::cout << std::string(114, '-') << "\n";
    }

    csv.close();
    std::cout << "\n[SUCCESS] Knapsack benchmark completed. Results written to: " << csv_path << "\n";
    std::cout << "All exact methods (DP, B&B, ILP) verified to return identical optima.\n";
    return 0;
}
