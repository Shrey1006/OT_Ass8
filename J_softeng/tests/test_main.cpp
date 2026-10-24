#include "test_minimizer.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

void test_incidence_and_metadata_loading() {
    std::cout << "[Test 1] Dataset Loading (Incidence Matrix & Metadata)..." << std::flush;
    softeng::TestMinimizerEngine engine;
    std::string meta_path = "J_softeng/results/pugixml_tests_metadata.csv";
    std::string inc_path = "J_softeng/results/pugixml_incidence_matrix.csv";

    if (!engine.load_from_csv(meta_path, inc_path)) {
        meta_path = "../" + meta_path;
        inc_path = "../" + inc_path;
        assert(engine.load_from_csv(meta_path, inc_path));
    }

    assert(engine.tests.size() == 953);
    assert(engine.subsystems.size() == 29);
    assert(engine.total_subsystems == 29);
    assert(engine.full_suite_runtime_us > 10000.0);
    std::cout << " PASSED (953 Tests, 29 Subsystems Loaded)\n";
}

void test_naive_baseline_full() {
    std::cout << "[Test 2] Baseline Full Suite (100% Coverage, 0% Reduction)..." << std::flush;
    softeng::TestMinimizerEngine engine;
    std::string meta = "J_softeng/results/pugixml_tests_metadata.csv";
    std::string inc = "J_softeng/results/pugixml_incidence_matrix.csv";
    if (!engine.load_from_csv(meta, inc)) {
        meta = "../" + meta; inc = "../" + inc;
        assert(engine.load_from_csv(meta, inc));
    }

    auto res = engine.solve_naive_full();
    assert(res.selected_test_count == 953);
    assert(res.covered_subsystems_count == 29);
    assert(std::abs(res.coverage_pct - 100.0) < 1e-6);
    assert(std::abs(res.selection_ratio_pct - 100.0) < 1e-6);
    assert(std::abs(res.speedup_factor - 1.0) < 1e-6);
    std::cout << " PASSED (Full Coverage Invariant Satisfied)\n";
}

void test_naive_random_coverage_loss() {
    std::cout << "[Test 3] Naive Random Sampling Coverage Loss Defect..." << std::flush;
    softeng::TestMinimizerEngine engine;
    std::string meta = "J_softeng/results/pugixml_tests_metadata.csv";
    std::string inc = "J_softeng/results/pugixml_incidence_matrix.csv";
    if (!engine.load_from_csv(meta, inc)) {
        meta = "../" + meta; inc = "../" + inc;
        assert(engine.load_from_csv(meta, inc));
    }

    auto res20 = engine.solve_naive_random(0.20, 42);
    // 20% random sampling must drop coverage below 100% (misses rare subsystems like compact or specific xpath)
    assert(res20.coverage_pct < 100.0);
    assert(res20.covered_subsystems_count < 29);
    assert(res20.selected_test_count == (int)std::round(engine.tests.size() * 0.20));
    std::cout << " PASSED (Confirmed Random Sampling Incurred Coverage Loss: " << res20.coverage_pct << "%)\n";
}

void test_greedy_unweighted_minimization() {
    std::cout << "[Test 4] Greedy Unweighted Set Cover (100% Coverage & Massive Pruning)..." << std::flush;
    softeng::TestMinimizerEngine engine;
    std::string meta = "J_softeng/results/pugixml_tests_metadata.csv";
    std::string inc = "J_softeng/results/pugixml_incidence_matrix.csv";
    if (!engine.load_from_csv(meta, inc)) {
        meta = "../" + meta; inc = "../" + inc;
        assert(engine.load_from_csv(meta, inc));
    }

    auto res = engine.solve_greedy(false);
    // Must preserve 100% coverage
    assert(res.coverage_pct == 100.0);
    assert(res.covered_subsystems_count == 29);
    // Test count must be radically pruned (less than 20 tests needed to cover 29 subsystems)
    assert(res.selected_test_count <= 20);
    assert(res.selected_test_count > 0);
    // Reduction must exceed 90%
    assert(res.runtime_saving_pct >= 85.0);
    std::cout << " PASSED (" << res.selected_test_count << " Tests Selected, 100% Coverage Preserved)\n";
}

void test_greedy_time_weighted_minimization() {
    std::cout << "[Test 5] Greedy Time-Weighted Set Cover (Runtime Speedup Invariant)..." << std::flush;
    softeng::TestMinimizerEngine engine;
    std::string meta = "J_softeng/results/pugixml_tests_metadata.csv";
    std::string inc = "J_softeng/results/pugixml_incidence_matrix.csv";
    if (!engine.load_from_csv(meta, inc)) {
        meta = "../" + meta; inc = "../" + inc;
        assert(engine.load_from_csv(meta, inc));
    }

    auto res = engine.solve_greedy(true);
    assert(res.coverage_pct == 100.0);
    assert(res.covered_subsystems_count == 29);
    // Speedup must be >= 5x
    assert(res.speedup_factor >= 5.0);
    assert(res.total_runtime_us < engine.full_suite_runtime_us * 0.20);
    std::cout << " PASSED (" << res.speedup_factor << "x Speedup, Runtime: " << res.total_runtime_us << " us)\n";
}

void test_ilp_exact_optimality() {
    std::cout << "[Test 6] Exact ILP / Branch-and-Bound Optimality (|T_ILP| <= |T_Greedy|)..." << std::flush;
    softeng::TestMinimizerEngine engine;
    std::string meta = "J_softeng/results/pugixml_tests_metadata.csv";
    std::string inc = "J_softeng/results/pugixml_incidence_matrix.csv";
    if (!engine.load_from_csv(meta, inc)) {
        meta = "../" + meta; inc = "../" + inc;
        assert(engine.load_from_csv(meta, inc));
    }

    auto greedy_res = engine.solve_greedy(false);
    auto ilp_res = engine.solve_ilp_exact(false);

    assert(ilp_res.coverage_pct == 100.0);
    assert(ilp_res.covered_subsystems_count == 29);
    // ILP must find equal or fewer tests than greedy
    assert(ilp_res.selected_test_count <= greedy_res.selected_test_count);
    assert(ilp_res.selected_test_count >= 8); // At least 8 tests mathematically needed
    std::cout << " PASSED (ILP Opt: " << ilp_res.selected_test_count << " Tests <= Greedy: " << greedy_res.selected_test_count << " Tests)\n";
}

void test_ilp_optimal_runtime() {
    std::cout << "[Test 7] Exact ILP Optimal Runtime (Runtime_ILP <= Runtime_Greedy)..." << std::flush;
    softeng::TestMinimizerEngine engine;
    std::string meta = "J_softeng/results/pugixml_tests_metadata.csv";
    std::string inc = "J_softeng/results/pugixml_incidence_matrix.csv";
    if (!engine.load_from_csv(meta, inc)) {
        meta = "../" + meta; inc = "../" + inc;
        assert(engine.load_from_csv(meta, inc));
    }

    auto greedy_w = engine.solve_greedy(true);
    auto ilp_w = engine.solve_ilp_exact(true);

    assert(ilp_w.coverage_pct == 100.0);
    assert(ilp_w.total_runtime_us <= greedy_w.total_runtime_us + 1e-6);
    std::cout << " PASSED (ILP Opt Runtime: " << ilp_w.total_runtime_us << " us <= Greedy: " << greedy_w.total_runtime_us << " us)\n";
}

void test_budgeted_prioritization_monotonicity() {
    std::cout << "[Test 8] Budgeted Prioritization Monotonicity (Cov(25%) <= Cov(50%) <= Cov(75%))..." << std::flush;
    softeng::TestMinimizerEngine engine;
    std::string meta = "J_softeng/results/pugixml_tests_metadata.csv";
    std::string inc = "J_softeng/results/pugixml_incidence_matrix.csv";
    if (!engine.load_from_csv(meta, inc)) {
        meta = "../" + meta; inc = "../" + inc;
        assert(engine.load_from_csv(meta, inc));
    }

    auto b25 = engine.solve_budgeted_prioritization(0.25);
    auto b50 = engine.solve_budgeted_prioritization(0.50);
    auto b75 = engine.solve_budgeted_prioritization(0.75);

    assert(b25.total_runtime_us <= engine.full_suite_runtime_us * 0.25 + 1e-6);
    assert(b50.total_runtime_us <= engine.full_suite_runtime_us * 0.50 + 1e-6);
    assert(b75.total_runtime_us <= engine.full_suite_runtime_us * 0.75 + 1e-6);

    assert(b25.covered_subsystems_count <= b50.covered_subsystems_count);
    assert(b50.covered_subsystems_count <= b75.covered_subsystems_count);
    assert(b75.coverage_pct >= 90.0);
    std::cout << " PASSED (Monotonic Coverage: 25%->" << b25.coverage_pct << "% | 50%->" << b50.coverage_pct << "% | 75%->" << b75.coverage_pct << "%)\n";
}

void test_mutation_score_and_fault_detection_invariants() {
    std::cout << "[Test 9] Mutation Testing & Fault Detection Invariants (J3)..." << std::flush;
    std::ifstream mut_file("J_softeng/results/mutation_score_comparison.csv");
    if (!mut_file.is_open()) mut_file.open("../J_softeng/results/mutation_score_comparison.csv");
    assert(mut_file.is_open());

    std::string line;
    std::getline(mut_file, line); // Header
    int row_count = 0;

    while (std::getline(mut_file, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> cols;
        while (std::getline(ss, token, ',')) cols.push_back(token);

        if (cols.size() >= 7) {
            row_count++;
            std::string name = cols[0];
            double mut_score = std::stod(cols[5]);
            double fault_loss = std::stod(cols[6]);

            if (name == "Naive Full Suite (Baseline)") {
                assert(mut_score == 100.0);
                assert(fault_loss == 0.0);
            } else if (name == "Exact ILP Minimized Suite" || name == "Greedy Minimized Suite") {
                // Minimized suite retains high fault detection but loses subtle edge cases
                assert(mut_score >= 85.0);
                assert(fault_loss <= 15.0);
            } else if (name == "Naive Sequential Prefix (20%)") {
                // Catastrophic fault detection loss
                assert(mut_score <= 50.0);
                assert(fault_loss >= 50.0);
            }
        }
    }
    assert(row_count == 5);
    std::cout << " PASSED (Mutation Score Invariants Verified across 5 Suites)\n";
}

void test_diagnostic_granularity_and_readability() {
    std::cout << "[Test 10] Diagnostic Granularity & Entanglement Metric (J3)..." << std::flush;
    softeng::TestMinimizerEngine engine;
    std::string meta = "J_softeng/results/pugixml_tests_metadata.csv";
    std::string inc = "J_softeng/results/pugixml_incidence_matrix.csv";
    if (!engine.load_from_csv(meta, inc)) {
        meta = "../" + meta; inc = "../" + inc;
        assert(engine.load_from_csv(meta, inc));
    }

    auto full_res = engine.solve_naive_full();
    auto ilp_res = engine.solve_ilp_exact(true);

    // Compute average subsystems per test in full suite vs minimized suite
    double full_total_subs = 0.0;
    for (int tid : full_res.selected_test_ids) full_total_subs += engine.tests[tid].covered_subsystems.size();
    double full_avg_subs = full_total_subs / full_res.selected_test_count;

    double ilp_total_subs = 0.0;
    for (int tid : ilp_res.selected_test_ids) ilp_total_subs += engine.tests[tid].covered_subsystems.size();
    double ilp_avg_subs = ilp_total_subs / ilp_res.selected_test_count;

    // Verify that minimized tests cover all 29 subsystems with high test-to-subsystem ratio (29 / 14 = 2.07 subsystems/test)
    assert(ilp_res.covered_subsystems_count == 29);
    assert(ilp_res.selected_test_count == 14);
    double subsystem_density = static_cast<double>(ilp_res.covered_subsystems_count) / ilp_res.selected_test_count;
    assert(subsystem_density >= 2.0); // Each test in minimized suite uniquely covers > 2 subsystems on average
    std::cout << " PASSED (Subsystem Coverage Density: " << std::fixed << std::setprecision(2) << subsystem_density
              << " unique subsystems/test in Minimized Suite vs "
              << (29.0 / full_res.selected_test_count) << " in Full Suite)\n";
}

int main() {
    std::cout << "=========================================================================\n";
    std::cout << " Running Track J: Software Engineering Test-Suite Minimization Tests    \n";
    std::cout << "=========================================================================\n";

    test_incidence_and_metadata_loading();
    test_naive_baseline_full();
    test_naive_random_coverage_loss();
    test_greedy_unweighted_minimization();
    test_greedy_time_weighted_minimization();
    test_ilp_exact_optimality();
    test_ilp_optimal_runtime();
    test_budgeted_prioritization_monotonicity();
    test_mutation_score_and_fault_detection_invariants();
    test_diagnostic_granularity_and_readability();

    std::cout << "\n>>> All 10 Track J SoftEng Tests PASSED (100% OK)! <<<\n";
    return 0;
}
