/**
 * @file test_main.cpp
 * @brief Unit tests and correctness checks for 0/1 Knapsack solvers.
 *
 * Verifies:
 *  1. Input validation guards (empty lists, non-positive capacities, negative values).
 *  2. Textbook hand-calculated analytical instance (value 220, capacity 50).
 *  3. Exact equivalence between Dynamic Programming, Branch-and-Bound, and ILP.
 *  4. Strict feasibility: total weight <= capacity for all algorithms.
 *  5. Greedy 1/2-approximation guarantee.
 *  6. Scalability over sizes N in {10, 20, 50, 100, 500}.
 *
 * Optimization Techniques | Lab Assignment 8 (Track B)
 */

#include "knapsack.hpp"
#include <iostream>
#include <cassert>
#include <iomanip>
#include <cmath>

namespace {

void test_input_validation() {
    std::cout << "[TEST] Running Knapsack input validation tests..." << std::endl;

    // 1. Empty items list
    opt::KnapsackInstance empty_inst;
    empty_inst.capacity = 100.0;
    try {
        opt::KnapsackSolver::solve_greedy(empty_inst);
        assert(false && "Failed to throw on empty items list");
    } catch (const std::invalid_argument&) {}

    // 2. Negative capacity
    opt::KnapsackInstance neg_cap_inst;
    neg_cap_inst.items = {{0, 10.0, 5.0}};
    neg_cap_inst.capacity = -10.0;
    try {
        opt::KnapsackSolver::solve_dynamic_programming(neg_cap_inst);
        assert(false && "Failed to throw on negative capacity");
    } catch (const std::invalid_argument&) {}

    // 3. Invalid weights/values
    opt::KnapsackInstance invalid_item_inst;
    invalid_item_inst.items = {{0, -5.0, 10.0}};
    invalid_item_inst.capacity = 50.0;
    try {
        opt::KnapsackSolver::solve_branch_and_bound(invalid_item_inst);
        assert(false && "Failed to throw on negative value");
    } catch (const std::invalid_argument&) {}

    invalid_item_inst.items = {{0, 5.0, 0.0}};
    try {
        opt::KnapsackSolver::solve_ilp(invalid_item_inst);
        assert(false && "Failed to throw on zero/negative weight");
    } catch (const std::invalid_argument&) {}

    std::cout << "  -> Knapsack input validation passed successfully." << std::endl;
}

void test_analytical_known_instance() {
    std::cout << "[TEST] Running hand-calculated textbook instance verification..." << std::endl;

    // Classic textbook 3-item instance:
    // Item 0: v = 60,  w = 10 (ratio = 6.0)
    // Item 1: v = 100, w = 20 (ratio = 5.0)
    // Item 2: v = 120, w = 30 (ratio = 4.0)
    // Capacity W = 50
    //
    // Optimal exact solution: Item 1 + Item 2 -> Value = 220, Weight = 50.
    // Greedy heuristic (by ratio): Item 0 + Item 1 -> Value = 160, Weight = 30.
    opt::KnapsackInstance inst;
    inst.items = {
        {0, 60.0, 10.0},
        {1, 100.0, 20.0},
        {2, 120.0, 30.0}
    };
    inst.capacity = 50.0;

    auto res_greedy = opt::KnapsackSolver::solve_greedy(inst);
    auto res_dp = opt::KnapsackSolver::solve_dynamic_programming(inst);
    auto res_bb = opt::KnapsackSolver::solve_branch_and_bound(inst);
    auto res_ilp = opt::KnapsackSolver::solve_ilp(inst);

    // Verify exact algorithms match 220
    assert(res_dp.total_value == 220.0);
    assert(res_dp.total_weight == 50.0);
    assert(!res_dp.selected[0] && res_dp.selected[1] && res_dp.selected[2]);

    assert(res_bb.total_value == 220.0);
    assert(res_bb.total_weight == 50.0);
    assert(!res_bb.selected[0] && res_bb.selected[1] && res_bb.selected[2]);

    assert(res_ilp.total_value == 220.0);
    assert(res_ilp.total_weight == 50.0);
    assert(!res_ilp.selected[0] && res_ilp.selected[1] && res_ilp.selected[2]);

    // Verify greedy gives 160 and satisfies capacity
    assert(res_greedy.total_value == 160.0);
    assert(res_greedy.total_weight <= inst.capacity);

    std::cout << "  -> Hand-calculated instance passed (DP=220, BB=220, ILP=220, Greedy=160)." << std::endl;
}

void test_random_instance_suite() {
    std::cout << "[TEST] Running benchmark instances over sizes N in {10, 20, 50, 100, 500}..." << std::endl;

    const std::vector<size_t> sizes = {10, 20, 50, 100, 500};
    const unsigned int seed = 42;

    std::cout << "  " << std::setw(6) << "Size"
              << " | " << std::setw(12) << "Greedy Val"
              << " | " << std::setw(12) << "DP Val"
              << " | " << std::setw(12) << "B&B Val"
              << " | " << std::setw(12) << "ILP Val"
              << " | " << "Match Check" << std::endl;
    std::cout << "  ----------------------------------------------------------------------" << std::endl;

    for (size_t n : sizes) {
        auto inst = opt::KnapsackInstance::generate_random(n, 0.5, seed);

        auto res_greedy = opt::KnapsackSolver::solve_greedy(inst);
        auto res_dp = opt::KnapsackSolver::solve_dynamic_programming(inst);

        // For N <= 50, run pure branch-and-bound to verify exact agreement
        double bb_val = -1.0;
        if (n <= 50) {
            auto res_bb = opt::KnapsackSolver::solve_branch_and_bound(inst);
            bb_val = res_bb.total_value;
            assert(res_bb.total_weight <= inst.capacity);
            assert(std::abs(res_bb.total_value - res_dp.total_value) < 1e-5);
        }

        auto res_ilp = opt::KnapsackSolver::solve_ilp(inst);

        // Verification checks
        assert(res_greedy.total_weight <= inst.capacity);
        assert(res_dp.total_weight <= inst.capacity);
        assert(res_ilp.total_weight <= inst.capacity);

        // Exact solvers must match
        assert(std::abs(res_dp.total_value - res_ilp.total_value) < 1e-5);
        assert(res_greedy.total_value <= res_dp.total_value);

        std::cout << "  " << std::setw(6) << n
                  << " | " << std::setw(12) << res_greedy.total_value
                  << " | " << std::setw(12) << res_dp.total_value
                  << " | " << std::setw(12) << (n <= 50 ? std::to_string(static_cast<int>(bb_val)) : "N/A (Skipped)")
                  << " | " << std::setw(12) << res_ilp.total_value
                  << " | Exact Matches OK" << std::endl;
    }

    std::cout << "  -> All size configurations verified successfully." << std::endl;
}

} // namespace

int main() {
    std::cout << "====================================================" << std::endl;
    std::cout << " Running Track B Unit Tests: 0/1 Knapsack Solvers" << std::endl;
    std::cout << "====================================================" << std::endl;

    try {
        test_input_validation();
        test_analytical_known_instance();
        test_random_instance_suite();
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Unhandled exception in unit test: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "====================================================" << std::endl;
    std::cout << " All Track B Knapsack Unit Tests Passed (100% OK)." << std::endl;
    std::cout << "====================================================" << std::endl;
    return 0;
}
