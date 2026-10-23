#include "ev_vrptw.hpp"
#include "ev_vrptw_metaheuristics.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <numeric>

struct ExperimentStats {
    std::string method_name;
    int num_runs = 0;
    std::vector<double> objectives;
    std::vector<double> distances;
    std::vector<double> vehicles;
    std::vector<double> tardinesses;
    std::vector<double> energies;
    std::vector<double> recharges;
    std::vector<double> runtimes_ms;
    int feasible_count = 0;

    void add_run(const vrptw::Solution& sol, double runtime_ms) {
        num_runs++;
        objectives.push_back(sol.objective_cost);
        distances.push_back(sol.total_distance);
        vehicles.push_back(static_cast<double>(sol.active_vehicles));
        tardinesses.push_back(sol.total_tardiness);
        energies.push_back(sol.total_energy);
        recharges.push_back(static_cast<double>(sol.total_recharges));
        runtimes_ms.push_back(runtime_ms);
        if (sol.is_feasible) feasible_count++;
    }

    static std::pair<double, double> mean_std(const std::vector<double>& v) {
        if (v.empty()) return {0.0, 0.0};
        double mean = std::accumulate(v.begin(), v.end(), 0.0) / v.size();
        double sq_sum = 0.0;
        for (double x : v) sq_sum += (x - mean) * (x - mean);
        double std = (v.size() > 1) ? std::sqrt(sq_sum / (v.size() - 1)) : 0.0;
        return {mean, std};
    }
};

int main() {
    std::cout << "=========================================================\n";
    std::cout << " Track E2: EV-VRPTW Strategy Comparison & Repeated Runs   \n";
    std::cout << "=========================================================\n";

    // Setup benchmark problem instance (25 customers, 4 stations, hybrid RC)
    vrptw::ProblemParams params;
    params.vehicle_capacity = 150.0;
    params.battery_capacity = 80.0;
    params.energy_consumption_base = 0.45;
    params.energy_consumption_load = 0.0015;
    params.recharge_rate = 30.0;
    params.speed = 40.0;
    params.cost_per_km = 1.20;
    params.fixed_cost_per_vehicle = 80.0;
    params.tardiness_penalty_rate = 2.50;

    vrptw::VRPTWInstance instance = vrptw::generate_solomon_instance(25, 4, "RC", 42, params);
    std::cout << "Loaded Benchmark Instance: " << instance.name << "\n"
              << "  Nodes: " << instance.nodes.size() 
              << " (Customers: " << instance.customer_ids().size()
              << ", Stations: " << instance.station_ids().size() << ")\n\n";

    // 1. Run Baseline (Greedy Nearest-Neighbor)
    std::cout << ">>> Running Baseline (Greedy Nearest Neighbor)..." << std::endl;
    auto t0 = std::chrono::steady_clock::now();
    vrptw::Solution baseline_sol = vrptw::solve_greedy_baseline(instance);
    auto t1 = std::chrono::steady_clock::now();
    double baseline_time_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    ExperimentStats stats_baseline;
    stats_baseline.method_name = "Greedy-Baseline";
    stats_baseline.add_run(baseline_sol, baseline_time_ms);

    std::cout << "Baseline Cost: $" << baseline_sol.objective_cost 
              << " (Dist: " << baseline_sol.total_distance << " km, Veh: " 
              << baseline_sol.active_vehicles << ", Time: " << baseline_time_ms << " ms)\n\n";

    // 2. Run Simulated Annealing (SA-MNS) across 10 deterministic seeds
    const int NUM_SEEDS = 10;
    ExperimentStats stats_sa;
    stats_sa.method_name = "Simulated-Annealing-MNS";

    std::cout << ">>> Running Method 1: Simulated Annealing (10 seeds)..." << std::endl;
    vrptw::SAParams sa_params;
    sa_params.initial_temp = 120.0;
    sa_params.cooling_rate = 0.985;
    sa_params.iterations_per_temp = 20;
    sa_params.max_iterations = 2000;

    vrptw::Solution best_sa_sol;
    best_sa_sol.objective_cost = std::numeric_limits<double>::infinity();

    for (int s = 1; s <= NUM_SEEDS; ++s) {
        auto start = std::chrono::steady_clock::now();
        vrptw::Solution sol = vrptw::solve_simulated_annealing(instance, baseline_sol, sa_params, 1000 + s * 77);
        auto end = std::chrono::steady_clock::now();
        double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

        stats_sa.add_run(sol, elapsed_ms);
        if (sol.objective_cost < best_sa_sol.objective_cost) {
            best_sa_sol = sol;
        }
        std::cout << "  [SA Run " << std::setw(2) << s << "] Cost: $" << std::fixed << std::setprecision(2)
                  << sol.objective_cost << " | Dist: " << sol.total_distance << " km | Veh: " 
                  << sol.active_vehicles << " | Time: " << elapsed_ms << " ms\n";
    }
    std::cout << "\n";

    // 3. Run Large Neighborhood Search (LNS) across 10 deterministic seeds
    ExperimentStats stats_lns;
    stats_lns.method_name = "Large-Neighborhood-Search";

    std::cout << ">>> Running Method 2: Large Neighborhood Search (10 seeds)..." << std::endl;
    vrptw::LNSParams lns_params;
    lns_params.max_iterations = 350;
    lns_params.min_destroy = 2;
    lns_params.max_destroy = 5;

    vrptw::Solution best_lns_sol;
    best_lns_sol.objective_cost = std::numeric_limits<double>::infinity();

    for (int s = 1; s <= NUM_SEEDS; ++s) {
        auto start = std::chrono::steady_clock::now();
        vrptw::Solution sol = vrptw::solve_large_neighborhood_search(instance, baseline_sol, lns_params, 2000 + s * 131);
        auto end = std::chrono::steady_clock::now();
        double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

        stats_lns.add_run(sol, elapsed_ms);
        if (sol.objective_cost < best_lns_sol.objective_cost) {
            best_lns_sol = sol;
        }
        std::cout << "  [LNS Run " << std::setw(2) << s << "] Cost: $" << std::fixed << std::setprecision(2)
                  << sol.objective_cost << " | Dist: " << sol.total_distance << " km | Veh: " 
                  << sol.active_vehicles << " | Time: " << elapsed_ms << " ms\n";
    }
    std::cout << "\n";

    // 4. Print Summary Comparison Table
    std::cout << "=======================================================================================================\n";
    std::cout << "                                  STATISTICAL COMPARISON TABLE (10 RUNS)                              \n";
    std::cout << "=======================================================================================================\n";
    std::cout << std::left << std::setw(26) << "Algorithm"
              << std::setw(22) << "Objective Cost ($)"
              << std::setw(20) << "Distance (km)"
              << std::setw(12) << "Vehicles"
              << std::setw(16) << "Tardiness (h)"
              << std::setw(15) << "Runtime (ms)"
              << std::setw(10) << "Feasible" << "\n";
    std::cout << "-------------------------------------------------------------------------------------------------------\n";

    auto print_row = [](const ExperimentStats& st) {
        auto [obj_m, obj_s] = ExperimentStats::mean_std(st.objectives);
        auto [dst_m, dst_s] = ExperimentStats::mean_std(st.distances);
        auto [veh_m, veh_s] = ExperimentStats::mean_std(st.vehicles);
        auto [tar_m, tar_s] = ExperimentStats::mean_std(st.tardinesses);
        auto [tim_m, tim_s] = ExperimentStats::mean_std(st.runtimes_ms);

        std::ostringstream obj_str, dst_str, tim_str;
        obj_str << std::fixed << std::setprecision(2) << obj_m << " +/- " << obj_s;
        dst_str << std::fixed << std::setprecision(1) << dst_m << " +/- " << dst_s;
        tim_str << std::fixed << std::setprecision(1) << tim_m << " +/- " << tim_s;

        std::cout << std::left << std::setw(26) << st.method_name
                  << std::setw(22) << obj_str.str()
                  << std::setw(20) << dst_str.str()
                  << std::setw(12) << veh_m
                  << std::setw(16) << tar_m
                  << std::setw(15) << tim_str.str()
                  << st.feasible_count << "/" << st.num_runs << "\n";
    };

    print_row(stats_baseline);
    print_row(stats_sa);
    print_row(stats_lns);
    std::cout << "=======================================================================================================\n";

    // 5. Export results to CSV
    std::string csv_path = "E_case_study/results/strategy_comparison.csv";
    std::ofstream csv(csv_path);
    if (csv.is_open()) {
        csv << "method,run_id,objective_cost,total_distance,active_vehicles,tardiness_hours,energy_kwh,recharges,runtime_ms,is_feasible\n";
        for (int i = 0; i < stats_baseline.num_runs; ++i) {
            csv << stats_baseline.method_name << "," << (i + 1) << ","
                << stats_baseline.objectives[i] << "," << stats_baseline.distances[i] << ","
                << stats_baseline.vehicles[i] << "," << stats_baseline.tardinesses[i] << ","
                << stats_baseline.energies[i] << "," << stats_baseline.recharges[i] << ","
                << stats_baseline.runtimes_ms[i] << ",true\n";
        }
        for (int i = 0; i < stats_sa.num_runs; ++i) {
            csv << stats_sa.method_name << "," << (i + 1) << ","
                << stats_sa.objectives[i] << "," << stats_sa.distances[i] << ","
                << stats_sa.vehicles[i] << "," << stats_sa.tardinesses[i] << ","
                << stats_sa.energies[i] << "," << stats_sa.recharges[i] << ","
                << stats_sa.runtimes_ms[i] << ",true\n";
        }
        for (int i = 0; i < stats_lns.num_runs; ++i) {
            csv << stats_lns.method_name << "," << (i + 1) << ","
                << stats_lns.objectives[i] << "," << stats_lns.distances[i] << ","
                << stats_lns.vehicles[i] << "," << stats_lns.tardinesses[i] << ","
                << stats_lns.energies[i] << "," << stats_lns.recharges[i] << ","
                << stats_lns.runtimes_ms[i] << ",true\n";
        }
        std::cout << "Strategy comparison CSV written to: " << csv_path << "\n";
    }

    // Save best solution JSONs
    vrptw::save_solution_to_json(best_sa_sol, "E_case_study/results/best_sa_solution.json");
    vrptw::save_solution_to_json(best_lns_sol, "E_case_study/results/best_lns_solution.json");

    return 0;
}
