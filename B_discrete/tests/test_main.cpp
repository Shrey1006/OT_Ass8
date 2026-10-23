/**
 * @file test_main.cpp
 * @brief Unit tests and correctness checks for 0/1 Knapsack, Max-Cut, and Feature Selection QUBO.
 *
 * Verifies:
 *  1. Input validation guards (empty lists, non-positive capacities, negative values).
 *  2. Textbook hand-calculated analytical instance (value 220, capacity 50).
 *  3. Exact equivalence between Dynamic Programming, Branch-and-Bound, and ILP.
 *  4. Strict feasibility: total weight <= capacity for all algorithms.
 *  5. Greedy 1/2-approximation guarantee.
 *  6. Scalability over sizes N in {10, 20, 50, 100, 500}.
 *  7. Exact algebraic equivalence between QUBO and Ising model under x = (1 + s)/2.
 *  8. Knapsack constraint encoding as a QUBO with quadratic penalty terms.
 *  9. Max-Cut on random graphs: Greedy vs Exhaustive vs Simulated Annealing.
 *  10. Export of Max-Cut QUBO to JSON file for Track C.
 *  11. Feature selection (choose k of n by mRMR): QUBO vs Greedy on analytical counter-example
 *      and synthetic benchmark (10 stochastic runs, Mean +/- Std).
 *
 * Optimization Techniques | Lab Assignment 8 (Track B)
 */

#include "knapsack.hpp"
#include "qubo.hpp"
#include "maxcut.hpp"
#include "feature_selection.hpp"
#include <iostream>
#include <cassert>
#include <iomanip>
#include <cmath>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(d) _mkdir(d)
#else
#include <sys/stat.h>
#define MKDIR(d) mkdir(d, 0755)
#endif

namespace {

void test_input_validation() {
    std::cout << "[TEST] Running Knapsack input validation tests..." << std::endl;

    opt::KnapsackInstance empty_inst;
    empty_inst.capacity = 100.0;
    try {
        opt::KnapsackSolver::solve_greedy(empty_inst);
        assert(false && "Failed to throw on empty items list");
    } catch (const std::invalid_argument&) {}

    opt::KnapsackInstance neg_cap_inst;
    neg_cap_inst.items = {{0, 10.0, 5.0}};
    neg_cap_inst.capacity = -10.0;
    try {
        opt::KnapsackSolver::solve_dynamic_programming(neg_cap_inst);
        assert(false && "Failed to throw on negative capacity");
    } catch (const std::invalid_argument&) {}

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

    assert(res_dp.total_value == 220.0);
    assert(res_dp.total_weight == 50.0);
    assert(!res_dp.selected[0] && res_dp.selected[1] && res_dp.selected[2]);

    assert(res_bb.total_value == 220.0);
    assert(res_bb.total_weight == 50.0);
    assert(!res_bb.selected[0] && res_bb.selected[1] && res_bb.selected[2]);

    assert(res_ilp.total_value == 220.0);
    assert(res_ilp.total_weight == 50.0);
    assert(!res_ilp.selected[0] && res_ilp.selected[1] && res_ilp.selected[2]);

    assert(res_greedy.total_value == 160.0);
    assert(res_greedy.total_weight <= inst.capacity);

    std::cout << "  -> Hand-calculated instance passed (DP=220, BB=220, ILP=220, Greedy=160)." << std::endl;
}

void test_random_instance_suite() {
    std::cout << "[TEST] Running benchmark instances over sizes N in {10, 20, 50, 100, 500}..." << std::endl;

    const std::vector<size_t> sizes = {10, 20, 50, 100, 500};
    const unsigned int seed = 42;

    for (size_t n : sizes) {
        auto inst = opt::KnapsackInstance::generate_random(n, 0.5, seed);

        auto res_greedy = opt::KnapsackSolver::solve_greedy(inst);
        auto res_dp = opt::KnapsackSolver::solve_dynamic_programming(inst);

        if (n <= 50) {
            auto res_bb = opt::KnapsackSolver::solve_branch_and_bound(inst);
            assert(res_bb.total_weight <= inst.capacity);
            assert(std::abs(res_bb.total_value - res_dp.total_value) < 1e-5);
        }

        auto res_ilp = opt::KnapsackSolver::solve_ilp(inst);

        assert(res_greedy.total_weight <= inst.capacity);
        assert(res_dp.total_weight <= inst.capacity);
        assert(res_ilp.total_weight <= inst.capacity);

        assert(std::abs(res_dp.total_value - res_ilp.total_value) < 1e-5);
        assert(res_greedy.total_value <= res_dp.total_value);
    }

    std::cout << "  -> All size configurations verified successfully." << std::endl;
}

void test_qubo_ising_conversion() {
    std::cout << "[TEST] Running explicit Binary-to-Spin x = (1 + s)/2 QUBO-Ising equivalence test..." << std::endl;

    opt::QUBO qubo(4);
    qubo.at(0, 0) = -2.0;
    qubo.at(1, 1) = 3.5;
    qubo.at(2, 2) = -1.2;
    qubo.at(3, 3) = 4.0;

    qubo.at(0, 1) = 1.5;
    qubo.at(0, 2) = -2.5;
    qubo.at(1, 3) = 3.0;
    qubo.at(2, 3) = -0.5;

    opt::IsingModel ising = qubo.to_ising();

    for (uint32_t mask = 0; mask < 16; ++mask) {
        std::vector<int> x(4);
        std::vector<int> s(4);

        for (size_t i = 0; i < 4; ++i) {
            x[i] = (mask >> i) & 1;
            s[i] = 2 * x[i] - 1;
            assert(x[i] == (1 + s[i]) / 2);
        }

        double e_qubo = qubo.evaluate(x);
        double e_ising = ising.energy(s);

        assert(std::abs(e_qubo - e_ising) < 1e-10);
    }

    std::cout << "  -> Exact algebraic equivalence E_QUBO(x) == E_Ising(s) confirmed for all states." << std::endl;
}

void test_knapsack_qubo_penalty() {
    std::cout << "[TEST] Running Knapsack QUBO penalty-term encoding verification..." << std::endl;

    std::vector<double> values = {60.0, 100.0, 120.0};
    std::vector<double> weights = {10.0, 20.0, 30.0};
    double capacity = 50.0;

    opt::QUBO knapsack_qubo = opt::QUBO::from_knapsack(values, weights, capacity, 240.0);
    auto res_bf = opt::QUBOSolver::solve_brute_force(knapsack_qubo);

    assert(res_bf.best_bitstring.size() >= 3);
    assert(res_bf.best_bitstring[0] == 0); // Item 0 not taken
    assert(res_bf.best_bitstring[1] == 1); // Item 1 taken
    assert(res_bf.best_bitstring[2] == 1); // Item 2 taken

    double total_wt = res_bf.best_bitstring[0] * weights[0] +
                      res_bf.best_bitstring[1] * weights[1] +
                      res_bf.best_bitstring[2] * weights[2];
    double total_val = res_bf.best_bitstring[0] * values[0] +
                       res_bf.best_bitstring[1] * values[1] +
                       res_bf.best_bitstring[2] * values[2];

    assert(total_wt == 50.0);
    assert(total_val == 220.0);

    std::cout << "  -> Knapsack QUBO with quadratic penalty recovered exact global optimum (val=220, wt=50)." << std::endl;
}

void test_maxcut_graph_algorithms() {
    std::cout << "[TEST] Running Max-Cut on Random Graph: Greedy, Exhaustive, QUBO, and Simulated Annealing..." << std::endl;

    auto graph = opt::MaxCutGraph::generate_erdos_renyi(6, 0.6, 1.0, 3.0, 42);
    assert(graph.num_nodes() == 6);
    assert(graph.num_edges() > 0);

    auto opt_res = graph.solve_exhaustive();
    double opt_cut = opt_res.first;
    assert(!opt_res.second.empty());

    auto greedy_res = graph.solve_greedy();
    double greedy_cut = greedy_res.first;
    assert(!greedy_res.second.empty());
    assert(greedy_cut <= opt_cut);
    assert(greedy_cut >= 0.5 * opt_cut);

    opt::QUBO qubo = graph.to_qubo();
    auto res_bf = opt::QUBOSolver::solve_brute_force(qubo);
    assert(std::abs(-res_bf.best_energy - opt_cut) < 1e-10);

    std::cout << "  Running 10 stochastic Simulated Annealing runs on Max-Cut QUBO..." << std::endl;
    double sa_cut_sum = 0.0;
    double sa_cut_sq_sum = 0.0;
    const size_t num_runs = 10;

    for (size_t s = 0; s < num_runs; ++s) {
        unsigned int seed = 201 + static_cast<unsigned int>(s);
        auto res_sa = opt::QUBOSolver::solve_simulated_annealing(qubo, 3000, 10.0, 0.01, seed);
        double cut_val = graph.compute_cut(res_sa.best_bitstring);
        sa_cut_sum += cut_val;
        sa_cut_sq_sum += cut_val * cut_val;
        assert(cut_val <= opt_cut);
    }

    double mean_cut = sa_cut_sum / static_cast<double>(num_runs);
    double var_cut = (sa_cut_sq_sum - num_runs * mean_cut * mean_cut) / static_cast<double>(num_runs - 1);
    double std_cut = std::sqrt(std::max(0.0, var_cut));

    std::cout << "  -> Max-Cut Results: Exact=" << opt_cut
              << " | Greedy=" << greedy_cut
              << " | Simulated Annealing (10 runs): " << mean_cut << " +/- " << std_cut << std::endl;

    std::string json_path = "B_discrete/results/maxcut_qubo.json";
    {
        std::ofstream probe(json_path);
        if (!probe.is_open()) {
            json_path = "results/maxcut_qubo.json";
        }
    }
    graph.export_to_json(json_path);

    std::cout << "  -> Max-Cut graph algorithms, QUBO mapping, and JSON export verified." << std::endl;
}

void test_feature_selection_qubo_vs_greedy() {
    std::cout << "[TEST] Running Feature Selection (mRMR): QUBO vs Greedy Comparison..." << std::endl;

    // 1. Analytical counter-example where Greedy forward selection fails:
    // 4 candidate features, choose k = 2, lambda = 1.0:
    // Feature 0: relevance = 0.90 (highest)
    // Feature 1: relevance = 0.85
    // Feature 2: relevance = 0.85
    // Feature 3: relevance = 0.10
    // Redundancy: C01 = 0.90, C02 = 0.90, C03 = 0.10, C12 = 0.00, C13 = 0.10, C23 = 0.10
    //
    // Greedy path:
    //   Step 1: picks Feature 0 (rel=0.90)
    //   Step 2: marginal gain for F1: 0.85 - 0.90 = -0.05
    //           marginal gain for F2: 0.85 - 0.90 = -0.05
    //           marginal gain for F3: 0.10 - 0.10 =  0.00 (highest gain!)
    //   Greedy picks {0, 3} -> Score = 0.90 + 0.10 - 0.10 = 0.90
    //
    // Global optimum (QUBO):
    //   Picks {1, 2} -> Score = 0.85 + 0.85 - 0.00 = 1.70 (88.9% better than Greedy!)
    {
        std::vector<double> rel = {0.90, 0.85, 0.85, 0.10};
        std::vector<std::vector<double>> red = {
            {0.00, 0.90, 0.90, 0.10},
            {0.90, 0.00, 0.00, 0.10},
            {0.90, 0.00, 0.00, 0.10},
            {0.10, 0.10, 0.10, 0.00}
        };

        opt::FeatureSelectionProblem fs(4, 2, rel, red, 1.0);

        // Greedy
        auto greedy_res = fs.solve_greedy();
        assert(greedy_res.is_feasible);
        assert(greedy_res.selected_features.size() == 2);
        assert(std::abs(greedy_res.objective_score - 0.90) < 1e-5);

        // QUBO formulation
        auto qubo_pair = fs.to_qubo(20.0);
        const auto& qubo = qubo_pair.first;
        auto qubo_res = opt::QUBOSolver::solve_brute_force(qubo);

        auto decoded_qubo = fs.decode_result("QUBO Brute-Force", qubo_res.best_bitstring, qubo_res.runtime_us);
        assert(decoded_qubo.is_feasible);
        assert(std::abs(decoded_qubo.objective_score - 1.70) < 1e-5);

        // Verify QUBO strictly beats Greedy
        assert(decoded_qubo.objective_score > greedy_res.objective_score);

        std::cout << "  -> Analytical instance: Greedy Score = " << greedy_res.objective_score
                  << " vs. QUBO Score = " << decoded_qubo.objective_score
                  << " (QUBO is " << ((decoded_qubo.objective_score - greedy_res.objective_score) / greedy_res.objective_score) * 100.0
                  << "% superior due to non-myopic global coupling)." << std::endl;
    }

    // 2. Synthetic benchmark problem (n = 12 candidate features, select k = 4):
    {
        auto fs_bench = opt::FeatureSelectionProblem::generate_synthetic(12, 4, 1.0, 42);

        auto greedy_res = fs_bench.solve_greedy();
        assert(greedy_res.is_feasible);

        auto qubo_pair = fs_bench.to_qubo();
        const auto& qubo = qubo_pair.first;

        auto bf_res = opt::QUBOSolver::solve_brute_force(qubo);
        auto decoded_bf = fs_bench.decode_result("QUBO Exact", bf_res.best_bitstring, bf_res.runtime_us);
        assert(decoded_bf.is_feasible);
        assert(decoded_bf.objective_score >= greedy_res.objective_score);

        // 10 stochastic Simulated Annealing runs (Rule 2)
        std::cout << "  Running 10 stochastic Simulated Annealing runs on Feature Selection QUBO..." << std::endl;
        const size_t num_runs = 10;
        double sa_score_sum = 0.0;
        double sa_score_sq_sum = 0.0;
        size_t feasible_count = 0;

        for (size_t s = 0; s < num_runs; ++s) {
            unsigned int seed = 301 + static_cast<unsigned int>(s);
            auto sa_res = opt::QUBOSolver::solve_simulated_annealing(qubo, 3000, 15.0, 0.01, seed);
            auto decoded_sa = fs_bench.decode_result("SA", sa_res.best_bitstring, sa_res.runtime_us);
            if (decoded_sa.is_feasible) {
                ++feasible_count;
                sa_score_sum += decoded_sa.objective_score;
                sa_score_sq_sum += decoded_sa.objective_score * decoded_sa.objective_score;
            }
        }

        assert(feasible_count == num_runs); // All 10 runs strictly satisfied cardinality constraint k=4
        double mean_sa = sa_score_sum / static_cast<double>(num_runs);
        double var_sa = (sa_score_sq_sum - num_runs * mean_sa * mean_sa) / static_cast<double>(num_runs - 1);
        double std_sa = std::sqrt(std::max(0.0, var_sa));

        std::cout << "  -> Synthetic Benchmark (n=12, k=4): Greedy Score = " << greedy_res.objective_score
                  << " | QUBO Exact = " << decoded_bf.objective_score
                  << " | QUBO Simulated Annealing (10 runs): " << mean_sa << " +/- " << std_sa << std::endl;
    }

    std::cout << "  -> Feature selection QUBO formulation and greedy comparison verified." << std::endl;
}

} // namespace

int main() {
    std::cout << "====================================================" << std::endl;
    std::cout << " Running Track B Unit Tests: Discrete & QUBO Solvers" << std::endl;
    std::cout << "====================================================" << std::endl;

    try {
        test_input_validation();
        test_analytical_known_instance();
        test_random_instance_suite();
        test_qubo_ising_conversion();
        test_knapsack_qubo_penalty();
        test_maxcut_graph_algorithms();
        test_feature_selection_qubo_vs_greedy();
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Unhandled exception in unit test: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "====================================================" << std::endl;
    std::cout << " All Track B Knapsack, Max-Cut & Feature Selection Tests Passed (100% OK)." << std::endl;
    std::cout << "====================================================" << std::endl;
    return 0;
}
