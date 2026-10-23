#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <cassert>
#include <fstream>
#include "max_coverage.hpp"
#include "fedavg.hpp"

void test_input_validation() {
    std::cout << "[TEST] Running input validation tests..." << std::endl;

    bool caught = false;
    try {
        opt::MaxCoverageInstance::generate_random(0, 10, 0.3, 42);
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    assert(caught);

    caught = false;
    try {
        opt::MaxCoverageInstance::generate_random(10, 0, 0.3, 42);
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    assert(caught);

    caught = false;
    try {
        opt::MaxCoverageInstance::generate_random(10, 10, -0.1, 42);
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    assert(caught);

    // Empty instance coverage
    opt::MaxCoverageInstance inst;
    inst.universe_size = 5;
    assert(opt::MaxCoverageSolver::evaluate_coverage(inst, {}) == 0.0);

    auto res = opt::MaxCoverageSolver::solve_greedy(inst, 0);
    assert(res.coverage_value == 0.0 && res.selected_subsets.empty());

    std::cout << "  -> Input validation tests passed successfully." << std::endl;
}

void test_submodularity_property_numerical() {
    std::cout << "[TEST] Testing submodularity (diminishing returns) property numerically on random sets..." << std::endl;

    const std::vector<std::pair<size_t, size_t>> test_configs = {
        {10, 30},
        {15, 50},
        {20, 80},
        {30, 100}
    };

    size_t total_tests = 0;
    size_t total_passed = 0;

    for (const auto& cfg : test_configs) {
        size_t n = cfg.first;
        size_t m = cfg.second;

        auto inst = opt::MaxCoverageInstance::generate_random(n, m, 0.25, 100 + static_cast<unsigned int>(n));

        // Test 1000 random subset pairs (A, B) where A subseteq B
        auto res = opt::MaxCoverageSolver::test_submodularity_property(inst, 1000, 200 + static_cast<unsigned int>(n));
        size_t passed = res.first;
        size_t violations = res.second;

        assert(violations == 0);
        assert(passed == 1000);

        total_tests += 1000;
        total_passed += passed;

        std::cout << "  -> Instance (n=" << n << ", m=" << m << "): 1000 random pairs tested: "
                  << passed << " satisfied, " << violations << " violations." << std::endl;
    }

    assert(total_passed == total_tests);
    std::cout << "  -> Submodularity diminishing returns confirmed across all " << total_tests << " random pairs (100% OK)." << std::endl;
}

void test_greedy_vs_bruteforce_and_bound() {
    std::cout << "[TEST] Comparing Greedy vs. Exact Brute Force and checking (1 - 1/e) bound..." << std::endl;

    const double one_minus_inv_e = 1.0 - 1.0 / std::exp(1.0); // ~ 0.6321205588
    std::cout << "  Theoretical (1 - 1/e) lower bound: " << std::fixed << std::setprecision(5) << one_minus_inv_e << "\n\n";

    struct TestCase {
        size_t n;
        size_t m;
        size_t k;
        double p;
        unsigned int seed;
    };

    const std::vector<TestCase> cases = {
        {10, 25, 2, 0.30, 101},
        {10, 25, 3, 0.30, 102},
        {10, 25, 4, 0.30, 103},
        {12, 35, 2, 0.25, 201},
        {12, 35, 3, 0.25, 202},
        {12, 35, 4, 0.25, 203},
        {14, 40, 3, 0.20, 301},
        {14, 40, 4, 0.20, 302},
        {16, 50, 3, 0.20, 401},
        {16, 50, 4, 0.20, 402}
    };

    std::cout << std::left
              << std::setw(6)  << "n"
              << std::setw(6)  << "m"
              << std::setw(6)  << "k"
              << std::setw(14) << "Greedy Cov"
              << std::setw(16) << "BruteForce Cov"
              << std::setw(14) << "Ratio (f/OPT)"
              << std::setw(14) << "(1-1/e) Bound"
              << std::setw(12) << "Satisfied?"
              << "\n";
    std::cout << std::string(88, '-') << "\n";

    for (const auto& tc : cases) {
        auto inst = opt::MaxCoverageInstance::generate_random(tc.n, tc.m, tc.p, tc.seed);

        auto greedy_res = opt::MaxCoverageSolver::solve_greedy(inst, tc.k);
        auto bf_res = opt::MaxCoverageSolver::solve_brute_force(inst, tc.k);

        double opt_cov = bf_res.coverage_value;
        double greedy_cov = greedy_res.coverage_value;
        double ratio = (opt_cov > 0.0) ? (greedy_cov / opt_cov) : 1.0;

        // Verify fundamental guarantees
        assert(greedy_cov <= opt_cov + 1e-9); // Greedy cannot exceed optimal
        assert(ratio >= one_minus_inv_e - 1e-9); // Must satisfy (1 - 1/e) approximation ratio
        assert(greedy_res.selected_subsets.size() <= tc.k);
        assert(bf_res.selected_subsets.size() <= tc.k);

        std::cout << std::left
                  << std::setw(6)  << tc.n
                  << std::setw(6)  << tc.m
                  << std::setw(6)  << tc.k
                  << std::setw(14) << std::fixed << std::setprecision(1) << greedy_cov
                  << std::setw(16) << std::fixed << std::setprecision(1) << opt_cov
                  << std::setw(14) << std::fixed << std::setprecision(4) << ratio
                  << std::setw(14) << std::fixed << std::setprecision(4) << one_minus_inv_e
                  << std::setw(12) << "YES (PASS)"
                  << "\n";
    }

    std::cout << std::string(88, '-') << "\n";
    std::cout << "  -> All test cases strictly satisfy the (1 - 1/e) Nemhauser-Wolsey-Fisher bound.\n";
}

void test_fedavg_vs_centralized() {
    std::cout << "\n[TEST] Running Track D3: FedAvg vs. Centralized Logistic Regression (10 Clients)...\n";

    const size_t num_clients = 10;
    const size_t n_train = 1500;
    const size_t n_test = 500;
    const size_t dim = 4;
    const unsigned int seed = 42;

    auto datasets = opt::Dataset::generate_train_test(n_train, n_test, dim, seed);
    const auto& train_data = datasets.first;
    const auto& test_data = datasets.second;

    auto iid_clients = opt::FederatedLearningSystem::partition_data(train_data, num_clients, false, seed);
    auto non_iid_clients = opt::FederatedLearningSystem::partition_data(train_data, num_clients, true, seed);

    std::string csv_path = "D_submodular_parallel/results/fedavg_benchmark.csv";
    std::ofstream csv(csv_path);
    if (!csv.is_open()) csv.open("results/fedavg_benchmark.csv");

    csv << "setting,method,local_steps,comm_rounds,total_gradient_steps,test_loss,test_accuracy,runtime_ms,comm_bytes\n";

    // 1. Centralized Baseline (500 steps)
    auto res_cent = opt::FederatedLearningSystem::train_centralized(train_data, test_data, 500, 0.15, 0.001);
    std::cout << "  -> Centralized SGD Baseline (500 steps): Test Loss = " << std::fixed << std::setprecision(4)
              << res_cent.final_test_loss << " | Accuracy = " << std::fixed << std::setprecision(2)
              << res_cent.final_test_accuracy << "% | Runtime = " << res_cent.runtime_ms << " ms\n\n";

    csv << "IID,Centralized,500,1,500," << res_cent.final_test_loss << "," << res_cent.final_test_accuracy << ","
        << res_cent.runtime_ms << ",0\n";

    std::cout << std::left
              << std::setw(10) << "Data Split"
              << std::setw(8)  << "Local E"
              << std::setw(8)  << "Rounds R"
              << std::setw(12) << "Total Steps"
              << std::setw(14) << "Test Loss"
              << std::setw(14) << "Accuracy (%)"
              << std::setw(14) << "Comm (Bytes)"
              << std::setw(14) << "Runtime (ms)"
              << "\n";
    std::cout << std::string(94, '-') << "\n";

    const std::vector<size_t> local_steps_list = {1, 5, 20};
    const std::vector<size_t> rounds_list = {5, 10, 25, 50};

    // Evaluate IID and Non-IID settings
    for (bool non_iid : {false, true}) {
        std::string split_name = non_iid ? "Non-IID" : "IID";
        const auto& clients = non_iid ? non_iid_clients : iid_clients;

        for (size_t E : local_steps_list) {
            for (size_t R : rounds_list) {
                auto res_fed = opt::FederatedLearningSystem::train_fedavg(clients, test_data, R, E, 0.15, 0.001);

                std::cout << std::left
                          << std::setw(10) << split_name
                          << std::setw(8)  << E
                          << std::setw(8)  << R
                          << std::setw(12) << res_fed.total_gradient_steps
                          << std::setw(14) << std::fixed << std::setprecision(4) << res_fed.final_test_loss
                          << std::setw(14) << std::fixed << std::setprecision(2) << res_fed.final_test_accuracy
                          << std::setw(14) << res_fed.communication_bytes
                          << std::setw(14) << std::fixed << std::setprecision(2) << res_fed.runtime_ms
                          << "\n";

                csv << split_name << ","
                    << "\"FedAvg\","
                    << E << ","
                    << R << ","
                    << res_fed.total_gradient_steps << ","
                    << res_fed.final_test_loss << ","
                    << res_fed.final_test_accuracy << ","
                    << res_fed.runtime_ms << ","
                    << res_fed.communication_bytes << "\n";
            }
            std::cout << std::string(94, '-') << "\n";
        }
    }

    csv.close();
    std::cout << "  -> FedAvg benchmark completed. Results written to: " << csv_path << "\n";
}

int main() {
    std::cout << "========================================================================\n";
    std::cout << "   Track D: Submodular Maximum Coverage & Federated Optimization        \n";
    std::cout << "========================================================================\n\n";

    test_input_validation();
    test_submodularity_property_numerical();
    test_greedy_vs_bruteforce_and_bound();
    test_fedavg_vs_centralized();

    std::cout << "\n[SUCCESS] All Track D tests passed (100% OK).\n";
    return 0;
}
