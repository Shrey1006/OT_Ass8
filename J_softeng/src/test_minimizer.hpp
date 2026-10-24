#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cmath>
#include <random>
#include <chrono>
#include <iomanip>
#include <cassert>

namespace softeng {

struct TestCase {
    int id;
    std::string name;
    std::string suite;
    double runtime_us;
    std::vector<int> covered_subsystems;
};

struct Subsystem {
    int id;
    std::string name;
    std::vector<int> covering_tests;
};

struct MinimizationResult {
    std::string algorithm_name;
    int total_tests;
    int selected_test_count;
    double selection_ratio_pct;
    double total_runtime_us;
    double runtime_saving_pct;
    double speedup_factor;
    int covered_subsystems_count;
    int total_subsystems_count;
    double coverage_pct;
    std::vector<int> selected_test_ids;
};

class TestMinimizerEngine {
public:
    std::vector<TestCase> tests;
    std::vector<Subsystem> subsystems;
    int total_subsystems = 0;
    double full_suite_runtime_us = 0.0;

    bool load_from_csv(const std::string& meta_path, const std::string& inc_path) {
        tests.clear();
        subsystems.clear();
        full_suite_runtime_us = 0.0;

        // 1. Read incidence matrix to get subsystems
        std::ifstream inc_file(inc_path);
        if (!inc_file.is_open()) {
            std::cerr << "[ERROR] Cannot open incidence matrix: " << inc_path << "\n";
            return false;
        }

        std::string line;
        // Read header
        if (!std::getline(inc_file, line)) return false;

        while (std::getline(inc_file, line)) {
            if (line.empty()) continue;
            std::stringstream ss(line);
            std::string s_id_str, s_name;
            std::getline(ss, s_id_str, ',');
            std::getline(ss, s_name, ',');

            Subsystem sub;
            sub.id = std::stoi(s_id_str);
            sub.name = s_name;

            std::string val_str;
            int test_idx = 0;
            while (std::getline(ss, val_str, ',')) {
                if (std::stoi(val_str) == 1) {
                    sub.covering_tests.push_back(test_idx);
                }
                test_idx++;
            }
            subsystems.push_back(sub);
        }
        total_subsystems = static_cast<int>(subsystems.size());

        // 2. Read metadata to get test cases
        std::ifstream meta_file(meta_path);
        if (!meta_file.is_open()) {
            std::cerr << "[ERROR] Cannot open metadata: " << meta_path << "\n";
            return false;
        }

        if (!std::getline(meta_file, line)) return false; // Header

        while (std::getline(meta_file, line)) {
            if (line.empty()) continue;
            std::stringstream ss(line);
            std::string token;
            std::vector<std::string> cols;

            while (std::getline(ss, token, ',')) cols.push_back(token);

            if (cols.size() >= 4) {
                TestCase tc;
                tc.id = std::stoi(cols[0]);
                tc.name = cols[1];
                tc.suite = cols[2];
                tc.runtime_us = std::stod(cols[3]);
                full_suite_runtime_us += tc.runtime_us;

                // Find subsystems covered by this test from subsystems list
                for (const auto& sub : subsystems) {
                    if (std::find(sub.covering_tests.begin(), sub.covering_tests.end(), tc.id) != sub.covering_tests.end()) {
                        tc.covered_subsystems.push_back(sub.id);
                    }
                }
                tests.push_back(tc);
            }
        }

        return !tests.empty() && !subsystems.empty();
    }

    MinimizationResult evaluate_selection(const std::string& name, const std::vector<int>& selected_ids) const {
        MinimizationResult res;
        res.algorithm_name = name;
        res.total_tests = static_cast<int>(tests.size());
        res.selected_test_count = static_cast<int>(selected_ids.size());
        res.selection_ratio_pct = (res.selected_test_count * 100.0) / res.total_tests;
        res.selected_test_ids = selected_ids;

        std::unordered_set<int> covered;
        double runtime = 0.0;

        for (int tid : selected_ids) {
            if (tid >= 0 && tid < static_cast<int>(tests.size())) {
                runtime += tests[tid].runtime_us;
                for (int sid : tests[tid].covered_subsystems) {
                    covered.insert(sid);
                }
            }
        }

        res.total_runtime_us = runtime;
        res.runtime_saving_pct = ((full_suite_runtime_us - runtime) * 100.0) / full_suite_runtime_us;
        res.speedup_factor = (runtime > 0.0) ? (full_suite_runtime_us / runtime) : 1.0;
        res.covered_subsystems_count = static_cast<int>(covered.size());
        res.total_subsystems_count = total_subsystems;
        res.coverage_pct = (total_subsystems > 0) ? (covered.size() * 100.0 / total_subsystems) : 0.0;

        return res;
    }

    // 1. Naive Baseline: Full Test Suite
    MinimizationResult solve_naive_full() const {
        std::vector<int> ids(tests.size());
        for (size_t i = 0; i < tests.size(); ++i) ids[i] = static_cast<int>(i);
        return evaluate_selection("Naive Full Suite (Baseline)", ids);
    }

    // 2. Naive Baseline: Random Sampling
    MinimizationResult solve_naive_random(double fraction, unsigned int seed = 42) const {
        std::vector<int> all_ids(tests.size());
        for (size_t i = 0; i < tests.size(); ++i) all_ids[i] = static_cast<int>(i);

        std::mt19937 rng(seed);
        std::shuffle(all_ids.begin(), all_ids.end(), rng);

        size_t k = std::max<size_t>(1, static_cast<size_t>(std::round(tests.size() * fraction)));
        std::vector<int> selected(all_ids.begin(), all_ids.begin() + k);
        std::sort(selected.begin(), selected.end());

        std::ostringstream oss;
        oss << "Naive Random (" << std::fixed << std::setprecision(0) << (fraction * 100.0) << "% Sampling)";
        return evaluate_selection(oss.str(), selected);
    }

    // 3. Naive Baseline: First-K Sequential Prefix
    MinimizationResult solve_naive_first_k(double fraction) const {
        size_t k = std::max<size_t>(1, static_cast<size_t>(std::round(tests.size() * fraction)));
        std::vector<int> selected(k);
        for (size_t i = 0; i < k; ++i) selected[i] = static_cast<int>(i);

        std::ostringstream oss;
        oss << "Naive Sequential Prefix (" << std::fixed << std::setprecision(0) << (fraction * 100.0) << "% Subset)";
        return evaluate_selection(oss.str(), selected);
    }

    // 4. Greedy Set Cover (Unweighted & Time-Weighted)
    MinimizationResult solve_greedy(bool time_weighted) const {
        std::unordered_set<int> uncovered;
        for (int sid = 0; sid < total_subsystems; ++sid) {
            uncovered.insert(sid);
        }

        std::vector<bool> test_chosen(tests.size(), false);
        std::vector<int> selected_ids;

        while (!uncovered.empty()) {
            int best_tid = -1;
            double best_score = -1.0;
            int max_new_covered = 0;

            for (size_t tid = 0; tid < tests.size(); ++tid) {
                if (test_chosen[tid]) continue;

                int new_covered = 0;
                for (int sid : tests[tid].covered_subsystems) {
                    if (uncovered.count(sid)) new_covered++;
                }

                if (new_covered == 0) continue;

                double score = 0.0;
                if (time_weighted) {
                    // Maximum marginal coverage per microsecond of execution time
                    score = static_cast<double>(new_covered) / tests[tid].runtime_us;
                } else {
                    // Maximum marginal coverage (pure set cover cardinality)
                    score = static_cast<double>(new_covered);
                }

                if (score > best_score || (std::abs(score - best_score) < 1e-9 && tests[tid].runtime_us < tests[best_tid].runtime_us)) {
                    best_score = score;
                    best_tid = static_cast<int>(tid);
                    max_new_covered = new_covered;
                }
            }

            if (best_tid == -1 || max_new_covered == 0) break; // All reachable covered

            test_chosen[best_tid] = true;
            selected_ids.push_back(best_tid);

            for (int sid : tests[best_tid].covered_subsystems) {
                uncovered.erase(sid);
            }
        }

        std::sort(selected_ids.begin(), selected_ids.end());
        std::string label = time_weighted ? "Greedy (Time-Weighted Set Cover)" : "Greedy (Unweighted Set Cover)";
        return evaluate_selection(label, selected_ids);
    }

    // 5. Exact ILP / Branch-and-Bound Solver with Column Dominance Reduction
    MinimizationResult solve_ilp_exact(bool time_weighted) const {
        // Step 1: Candidate Pool Reduction via Equivalence Classes & Column Dominance
        // Map unique sorted subsystem signatures to the test with minimal cost
        std::unordered_map<std::string, int> sig_to_best_test;
        for (size_t tid = 0; tid < tests.size(); ++tid) {
            std::vector<int> sig = tests[tid].covered_subsystems;
            std::sort(sig.begin(), sig.end());
            if (sig.empty()) continue;

            std::string sig_str;
            for (int s : sig) sig_str += std::to_string(s) + ",";

            if (sig_to_best_test.find(sig_str) == sig_to_best_test.end()) {
                sig_to_best_test[sig_str] = static_cast<int>(tid);
            } else {
                int old_tid = sig_to_best_test[sig_str];
                double old_cost = time_weighted ? tests[old_tid].runtime_us : 1.0;
                double new_cost = time_weighted ? tests[tid].runtime_us : 1.0;
                if (new_cost < old_cost) {
                    sig_to_best_test[sig_str] = static_cast<int>(tid);
                }
            }
        }

        std::vector<int> candidate_pool;
        for (const auto& kv : sig_to_best_test) {
            candidate_pool.push_back(kv.second);
        }

        // Column dominance filter: if candidate A's coverage is subset of candidate B and cost(B) <= cost(A), discard A
        std::vector<int> filtered_pool;
        for (size_t i = 0; i < candidate_pool.size(); ++i) {
            int tid_a = candidate_pool[i];
            double cost_a = time_weighted ? tests[tid_a].runtime_us : 1.0;
            std::unordered_set<int> set_a(tests[tid_a].covered_subsystems.begin(), tests[tid_a].covered_subsystems.end());
            bool dominated = false;

            for (size_t j = 0; j < candidate_pool.size(); ++j) {
                if (i == j) continue;
                int tid_b = candidate_pool[j];
                double cost_b = time_weighted ? tests[tid_b].runtime_us : 1.0;
                if (cost_b > cost_a) continue;

                // Check if set_a is subset of set_b
                bool is_subset = true;
                for (int s : set_a) {
                    if (std::find(tests[tid_b].covered_subsystems.begin(), tests[tid_b].covered_subsystems.end(), s) == tests[tid_b].covered_subsystems.end()) {
                        is_subset = false;
                        break;
                    }
                }
                if (is_subset && (cost_b < cost_a || (std::abs(cost_b - cost_a) < 1e-9 && tid_b < tid_a))) {
                    dominated = true;
                    break;
                }
            }

            if (!dominated) filtered_pool.push_back(tid_a);
        }

        // Subsystems to filtered candidates mapping
        std::vector<std::vector<int>> sub_to_filtered(total_subsystems);
        for (int tid : filtered_pool) {
            for (int sid : tests[tid].covered_subsystems) {
                sub_to_filtered[sid].push_back(tid);
            }
        }

        // Identify Essential Tests
        std::vector<bool> in_solution(tests.size(), false);
        std::vector<int> base_selected;
        std::unordered_set<int> initial_covered;

        for (int sid = 0; sid < total_subsystems; ++sid) {
            if (sub_to_filtered[sid].size() == 1) {
                int tid = sub_to_filtered[sid][0];
                if (!in_solution[tid]) {
                    in_solution[tid] = true;
                    base_selected.push_back(tid);
                    for (int s : tests[tid].covered_subsystems) {
                        initial_covered.insert(s);
                    }
                }
            }
        }

        // Remaining uncovered subsystems sorted by minimum candidate degree (fail-first heuristic)
        std::vector<int> rem_subsystems;
        for (int sid = 0; sid < total_subsystems; ++sid) {
            if (!initial_covered.count(sid)) {
                rem_subsystems.push_back(sid);
            }
        }
        std::sort(rem_subsystems.begin(), rem_subsystems.end(), [&](int a, int b) {
            return sub_to_filtered[a].size() < sub_to_filtered[b].size();
        });

        // Initial Upper Bound from Greedy
        MinimizationResult greedy_res = solve_greedy(time_weighted);
        double best_cost = time_weighted ? greedy_res.total_runtime_us : static_cast<double>(greedy_res.selected_test_count);
        std::vector<int> best_solution = greedy_res.selected_test_ids;

        // Recursive Branch and Bound
        double base_cost = 0.0;
        for (int tid : base_selected) {
            base_cost += time_weighted ? tests[tid].runtime_us : 1.0;
        }

        std::vector<int> current_selection = base_selected;

        auto solve_rec = [&](auto& self, size_t sub_idx, double cur_cost, std::unordered_set<int>& cur_covered) -> void {
            if (cur_cost >= best_cost) return;

            // Skip already covered subsystems
            while (sub_idx < rem_subsystems.size() && cur_covered.count(rem_subsystems[sub_idx])) {
                sub_idx++;
            }

            if (sub_idx >= rem_subsystems.size()) {
                best_cost = cur_cost;
                best_solution = current_selection;
                return;
            }

            int target_sid = rem_subsystems[sub_idx];
            const auto& cand_tests = sub_to_filtered[target_sid];

            // Branch on candidate tests covering target_sid
            for (int tid : cand_tests) {
                if (in_solution[tid]) continue;

                double add_cost = time_weighted ? tests[tid].runtime_us : 1.0;
                if (cur_cost + add_cost >= best_cost) continue;

                in_solution[tid] = true;
                current_selection.push_back(tid);

                std::vector<int> newly_added;
                for (int s : tests[tid].covered_subsystems) {
                    if (!cur_covered.count(s)) {
                        cur_covered.insert(s);
                        newly_added.push_back(s);
                    }
                }

                self(self, sub_idx + 1, cur_cost + add_cost, cur_covered);

                for (int s : newly_added) cur_covered.erase(s);
                current_selection.pop_back();
                in_solution[tid] = false;
            }
        };

        std::unordered_set<int> cur_cov = initial_covered;
        solve_rec(solve_rec, 0, base_cost, cur_cov);

        std::sort(best_solution.begin(), best_solution.end());
        std::string label = time_weighted ? "Exact ILP (Optimal Runtime Set Cover)" : "Exact ILP (Minimum Test Count Set Cover)";
        return evaluate_selection(label, best_solution);
    }

    // 6. Time-Budgeted Prioritization (Knapsack Formulation)
    MinimizationResult solve_budgeted_prioritization(double budget_ratio) const {
        double max_budget_us = full_suite_runtime_us * budget_ratio;
        std::unordered_set<int> covered;
        std::vector<bool> test_chosen(tests.size(), false);
        std::vector<int> selected_ids;
        double current_time_us = 0.0;

        while (true) {
            int best_tid = -1;
            double best_density = -1.0;

            for (size_t tid = 0; tid < tests.size(); ++tid) {
                if (test_chosen[tid]) continue;
                if (current_time_us + tests[tid].runtime_us > max_budget_us) continue;

                int marginal_gain = 0;
                for (int sid : tests[tid].covered_subsystems) {
                    if (!covered.count(sid)) marginal_gain++;
                }

                double density = static_cast<double>(marginal_gain) / tests[tid].runtime_us;
                if (density > best_density) {
                    best_density = density;
                    best_tid = static_cast<int>(tid);
                }
            }

            if (best_tid == -1) break; // No further tests fit within budget

            test_chosen[best_tid] = true;
            selected_ids.push_back(best_tid);
            current_time_us += tests[best_tid].runtime_us;

            for (int sid : tests[best_tid].covered_subsystems) {
                covered.insert(sid);
            }
        }

        std::sort(selected_ids.begin(), selected_ids.end());
        std::ostringstream oss;
        oss << "Budgeted Prioritization (" << std::fixed << std::setprecision(0) << (budget_ratio * 100.0) << "% Time Budget)";
        return evaluate_selection(oss.str(), selected_ids);
    }
};

} // namespace softeng
