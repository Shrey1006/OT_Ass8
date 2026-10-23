#include "ev_vrptw.hpp"
#include <iostream>

int main() {
    std::cout << "=========================================================\n";
    std::cout << " Track E: EV-VRPTW Benchmark Instance & Baseline Runner   \n";
    std::cout << "=========================================================\n";

    // 1. Generate standard Solomon-style EV-VRPTW instance (25 customers, 4 recharge stations, seed 42)
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

    std::cout << "[Step 1] Generating deterministic RC-25 instance (seed=42)...\n";
    vrptw::VRPTWInstance instance_25 = vrptw::generate_solomon_instance(25, 4, "RC", 42, params);

    std::cout << "Instance created: " << instance_25.name << "\n";
    std::cout << "  - Total Nodes: " << instance_25.nodes.size() 
              << " (1 Depot, " << instance_25.customer_ids().size() << " Customers, " 
              << instance_25.station_ids().size() << " Recharge Stations)\n";
    std::cout << "  - Vehicle Capacity: " << instance_25.params.vehicle_capacity << " kg\n";
    std::cout << "  - Battery Capacity: " << instance_25.params.battery_capacity << " kWh\n\n";

    // 2. Solve with Greedy Baseline
    std::cout << "[Step 2] Executing Greedy Nearest Neighbor Baseline Heuristic...\n";
    vrptw::Solution sol_25 = vrptw::solve_greedy_baseline(instance_25);

    // 3. Print Results
    vrptw::print_solution_summary(sol_25);

    // 4. Save JSON artifacts
    std::string inst_path = "E_case_study/results/instance_rc25.json";
    std::string sol_path = "E_case_study/results/baseline_solution_rc25.json";
    vrptw::save_instance_to_json(instance_25, inst_path);
    vrptw::save_solution_to_json(sol_25, sol_path);
    std::cout << "Artifacts saved successfully to:\n"
              << "  - " << inst_path << "\n"
              << "  - " << sol_path << "\n\n";

    // 5. Also run on small 10-customer instance for fast verification
    std::cout << "[Step 3] Executing fast verification on 10-customer instance...\n";
    vrptw::VRPTWInstance instance_10 = vrptw::generate_solomon_instance(10, 2, "C", 100, params);
    vrptw::Solution sol_10 = vrptw::solve_greedy_baseline(instance_10);
    vrptw::print_solution_summary(sol_10);

    return 0;
}
