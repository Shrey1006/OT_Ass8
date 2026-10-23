#include "ev_vrptw.hpp"
#include "ev_vrptw_metaheuristics.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

void test_deterministic_generator() {
    std::cout << "[Test 1] Deterministic Generator Consistency..." << std::flush;
    auto inst1 = vrptw::generate_solomon_instance(20, 3, "RC", 12345);
    auto inst2 = vrptw::generate_solomon_instance(20, 3, "RC", 12345);

    assert(inst1.nodes.size() == inst2.nodes.size());
    for (size_t i = 0; i < inst1.nodes.size(); ++i) {
        assert(inst1.nodes[i].id == inst2.nodes[i].id);
        assert(std::abs(inst1.nodes[i].x - inst2.nodes[i].x) < 1e-9);
        assert(std::abs(inst1.nodes[i].y - inst2.nodes[i].y) < 1e-9);
        assert(std::abs(inst1.nodes[i].demand - inst2.nodes[i].demand) < 1e-9);
        assert(std::abs(inst1.nodes[i].ready_time - inst2.nodes[i].ready_time) < 1e-9);
        assert(std::abs(inst1.nodes[i].due_time - inst2.nodes[i].due_time) < 1e-9);
    }
    std::cout << " PASSED\n";
}

void test_input_validation() {
    std::cout << "[Test 2] Input Validation & Error Handling..." << std::flush;
    vrptw::VRPTWInstance inst;
    bool caught_empty = false;
    try {
        inst.validate();
    } catch (const std::runtime_error&) {
        caught_empty = true;
    }
    assert(caught_empty);

    inst = vrptw::generate_solomon_instance(5, 2, "R", 42);
    inst.nodes[1].demand = inst.params.vehicle_capacity + 50.0;
    bool caught_overload = false;
    try {
        inst.validate();
    } catch (const std::runtime_error&) {
        caught_overload = true;
    }
    assert(caught_overload);

    inst = vrptw::generate_solomon_instance(5, 2, "R", 42);
    inst.nodes[2].ready_time = 10.0;
    inst.nodes[2].due_time = 5.0;
    bool caught_window = false;
    try {
        inst.validate();
    } catch (const std::runtime_error&) {
        caught_window = true;
    }
    assert(caught_window);

    std::cout << " PASSED\n";
}

void test_solution_validator() {
    std::cout << "[Test 3] Solution Feasibility Validator..." << std::flush;
    auto inst = vrptw::generate_solomon_instance(10, 2, "C", 42);

    vrptw::Solution sol = vrptw::solve_greedy_baseline(inst);
    assert(sol.is_feasible);
    assert(sol.active_vehicles > 0);
    assert(sol.total_distance > 0.0);
    assert(sol.total_energy > 0.0);

    if (sol.routes.size() >= 1 && sol.routes[0].stops.size() > 3) {
        vrptw::Solution corrupt_sol = sol;
        corrupt_sol.routes[0].stops[1].node_id = corrupt_sol.routes[0].stops[2].node_id;
        bool valid = vrptw::validate_solution(inst, corrupt_sol, false);
        assert(!valid);
    }

    {
        vrptw::Solution corrupt_sol = sol;
        corrupt_sol.routes[0].stops[0].node_id = 1;
        bool valid = vrptw::validate_solution(inst, corrupt_sol, false);
        assert(!valid);
    }

    std::cout << " PASSED\n";
}

void test_simulated_annealing() {
    std::cout << "[Test 4] Simulated Annealing Optimization..." << std::flush;
    auto inst = vrptw::generate_solomon_instance(15, 3, "RC", 42);
    vrptw::Solution baseline_sol = vrptw::solve_greedy_baseline(inst);
    assert(baseline_sol.is_feasible);

    vrptw::SAParams sa_params;
    sa_params.initial_temp = 50.0;
    sa_params.cooling_rate = 0.95;
    sa_params.iterations_per_temp = 10;
    sa_params.max_iterations = 300;

    vrptw::Solution sa_sol = vrptw::solve_simulated_annealing(inst, baseline_sol, sa_params, 1234);
    assert(sa_sol.is_feasible);
    assert(sa_sol.active_vehicles > 0);
    assert(sa_sol.objective_cost <= baseline_sol.objective_cost + 1e-4);
    std::cout << " PASSED (Cost: $" << sa_sol.objective_cost << " <= Baseline: $" << baseline_sol.objective_cost << ")\n";
}

void test_large_neighborhood_search() {
    std::cout << "[Test 5] Large Neighborhood Search Optimization..." << std::flush;
    auto inst = vrptw::generate_solomon_instance(15, 3, "RC", 42);
    vrptw::Solution baseline_sol = vrptw::solve_greedy_baseline(inst);
    assert(baseline_sol.is_feasible);

    vrptw::LNSParams lns_params;
    lns_params.max_iterations = 100;
    lns_params.min_destroy = 2;
    lns_params.max_destroy = 4;

    vrptw::Solution lns_sol = vrptw::solve_large_neighborhood_search(inst, baseline_sol, lns_params, 5678);
    assert(lns_sol.is_feasible);
    assert(lns_sol.active_vehicles > 0);
    assert(lns_sol.objective_cost <= baseline_sol.objective_cost + 1e-4);
    std::cout << " PASSED (Cost: $" << lns_sol.objective_cost << " <= Baseline: $" << baseline_sol.objective_cost << ")\n";
}

int main() {
    std::cout << "=========================================================\n";
    std::cout << " Running EV-VRPTW Unit & Integration Tests               \n";
    std::cout << "=========================================================\n";

    test_deterministic_generator();
    test_input_validation();
    test_solution_validator();
    test_simulated_annealing();
    test_large_neighborhood_search();

    std::cout << "\n>>> All EV-VRPTW tests passed successfully! <<<\n";
    return 0;
}
