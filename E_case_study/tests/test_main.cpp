#include "ev_vrptw.hpp"
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
    // Empty instance
    bool caught_empty = false;
    try {
        inst.validate();
    } catch (const std::runtime_error&) {
        caught_empty = true;
    }
    assert(caught_empty);

    // Demand exceeding capacity
    inst = vrptw::generate_solomon_instance(5, 2, "R", 42);
    inst.nodes[1].demand = inst.params.vehicle_capacity + 50.0;
    bool caught_overload = false;
    try {
        inst.validate();
    } catch (const std::runtime_error&) {
        caught_overload = true;
    }
    assert(caught_overload);

    // Inverted time window
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

    // 1. Valid baseline solution
    vrptw::Solution sol = vrptw::solve_greedy_baseline(inst);
    assert(sol.is_feasible);
    assert(sol.active_vehicles > 0);
    assert(sol.total_distance > 0.0);
    assert(sol.total_energy > 0.0);

    // 2. Corrupt solution: customer visited twice / another skipped
    if (sol.routes.size() >= 1 && sol.routes[0].stops.size() > 3) {
        vrptw::Solution corrupt_sol = sol;
        corrupt_sol.routes[0].stops[1].node_id = corrupt_sol.routes[0].stops[2].node_id;
        bool valid = vrptw::validate_solution(inst, corrupt_sol, false);
        assert(!valid);
    }

    // 3. Corrupt solution: disconnect start from depot
    {
        vrptw::Solution corrupt_sol = sol;
        corrupt_sol.routes[0].stops[0].node_id = 1; // Not depot
        bool valid = vrptw::validate_solution(inst, corrupt_sol, false);
        assert(!valid);
    }

    std::cout << " PASSED\n";
}

void test_baseline_execution() {
    std::cout << "[Test 4] Greedy Baseline Correctness & Coverage..." << std::flush;
    for (int n : {5, 15, 30}) {
        auto inst = vrptw::generate_solomon_instance(n, 3, "RC", 999);
        vrptw::Solution sol = vrptw::solve_greedy_baseline(inst);

        assert(sol.is_feasible);
        assert(sol.active_vehicles <= inst.params.max_vehicles);
        
        // Ensure every customer ID was visited
        std::vector<bool> visited(inst.nodes.size(), false);
        for (const auto& route : sol.routes) {
            for (const auto& stop : route.stops) {
                visited[stop.node_id] = true;
            }
        }
        for (int cid : inst.customer_ids()) {
            assert(visited[cid]);
        }
    }
    std::cout << " PASSED\n";
}

int main() {
    std::cout << "=========================================================\n";
    std::cout << " Running EV-VRPTW Unit & Integration Tests               \n";
    std::cout << "=========================================================\n";

    test_deterministic_generator();
    test_input_validation();
    test_solution_validator();
    test_baseline_execution();

    std::cout << "\n>>> All EV-VRPTW tests passed successfully! <<<\n";
    return 0;
}
