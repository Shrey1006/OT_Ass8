#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <cmath>
#include <cassert>

namespace dbms {

struct CandidateIndex {
    int id;
    std::string name;
    std::string table;
    std::string columns;
    int size_kb;              // Storage cost s_i in KB
    double baseline_cost;     // EXPLAIN cost without index
    double optimized_cost;    // EXPLAIN cost with index
    double benefit;           // Benefit b_i = baseline - optimized
    double density;           // Density rho_i = benefit / size_kb
};

struct SelectionResult {
    std::string method_name;
    double total_benefit;
    int total_size_kb;
    int budget_kb;
    double runtime_us;
    std::vector<int> selected_index_ids;
    std::vector<std::string> selected_index_names;
    bool optimal;
};

class IndexSelectionProblem {
public:
    std::vector<CandidateIndex> candidates;

    IndexSelectionProblem() = default;

    // Load candidate indexes from CSV file
    bool load_from_csv(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return false;
        }

        std::string line;
        // Skip header
        if (!std::getline(file, line)) return false;

        candidates.clear();
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            std::stringstream ss(line);
            std::string token;
            CandidateIndex idx;

            // id,name,table,columns,size_kb,baseline_cost,optimized_cost,benefit,density
            if (!std::getline(ss, token, ',')) continue;
            idx.id = std::stoi(token);

            if (!std::getline(ss, token, ',')) continue;
            idx.name = token;

            if (!std::getline(ss, token, ',')) continue;
            idx.table = token;

            // columns might be quoted with commas inside
            if (ss.peek() == '"') {
                ss.get(); // consume opening quote
                std::getline(ss, token, '"');
                idx.columns = token;
                if (ss.peek() == ',') ss.get(); // consume comma
            } else {
                if (!std::getline(ss, token, ',')) continue;
                idx.columns = token;
            }

            if (!std::getline(ss, token, ',')) continue;
            idx.size_kb = std::stoi(token);

            if (!std::getline(ss, token, ',')) continue;
            idx.baseline_cost = std::stod(token);

            if (!std::getline(ss, token, ',')) continue;
            idx.optimized_cost = std::stod(token);

            if (!std::getline(ss, token, ',')) continue;
            idx.benefit = std::stod(token);

            if (!std::getline(ss, token, ',')) continue;
            idx.density = std::stod(token);

            candidates.push_back(idx);
        }
        return !candidates.empty();
    }

    // Populate with built-in verified TPC-H SF 0.1 candidate index catalog
    void load_default_tpch_candidates() {
        candidates = {
            {1,  "idx_lineitem_shipdate",               "lineitem", "l_shipdate",                          4248,  18262.81, 14777.13, 3485.68, 0.8205},
            {2,  "idx_orders_orderdate",                "orders",   "o_orderdate",                         1040,  4988.76,  2903.74,  2085.02, 2.0048},
            {3,  "idx_customer_mktsegment",             "customer", "c_mktsegment",                        120,   556.29,   78.52,    477.77,  3.9814},
            {4,  "idx_lineitem_receiptdate",            "lineitem", "l_receiptdate",                       4248,  16993.91, 12501.67, 4492.24, 1.0575},
            {5,  "idx_lineitem_partkey",                "lineitem", "l_partkey",                           4424,  16528.94, 9917.36,  6611.58, 1.4945},
            {6,  "idx_orders_custkey",                  "orders",   "o_custkey",                           1248,  5495.16,  3297.10,  2198.06, 1.7613},
            {7,  "idx_partsupp_suppkey",                "partsupp", "ps_suppkey",                          552,   2369.87,  280.13,   2089.74, 3.7858},
            {8,  "idx_supplier_nationkey",              "supplier", "s_nationkey",                         16,    56.59,    47.74,    8.85,    0.5531},
            {9,  "idx_lineitem_shipdate_discount",      "lineitem", "l_shipdate, l_discount, l_quantity",  18528, 18268.95, 15657.43, 2611.52, 0.1409},
            {10, "idx_orders_orderdate_shippriority",   "orders",   "o_orderdate, o_shippriority",         1040,  5337.00,  2980.34,  2356.66, 2.2660},
            {11, "idx_customer_nationkey",              "customer", "c_nationkey",                         120,   550.06,   16.70,    533.36,  4.4447},
            {12, "idx_part_type_size",                  "part",     "p_type, p_size",                      480,   716.17,   411.46,   304.71,  0.6348}
        };
    }

    // -------------------------------------------------------------------------
    // 1. GREEDY SOLVER
    // Sorts candidate indexes by benefit-to-size density (benefit / size_kb)
    // Complexity: O(n log n) time, O(n) space
    // -------------------------------------------------------------------------
    SelectionResult solve_greedy(int budget_kb) const {
        auto t_start = std::chrono::high_resolution_clock::now();

        std::vector<CandidateIndex> sorted = candidates;
        std::sort(sorted.begin(), sorted.end(), [](const CandidateIndex& a, const CandidateIndex& b) {
            double r_a = a.size_kb > 0 ? (a.benefit / a.size_kb) : 0.0;
            double r_b = b.size_kb > 0 ? (b.benefit / b.size_kb) : 0.0;
            return r_a > r_b;
        });

        SelectionResult res;
        res.method_name = "Greedy (Density)";
        res.budget_kb = budget_kb;
        res.total_benefit = 0.0;
        res.total_size_kb = 0;
        res.optimal = false; // Heuristic guarantee

        int current_weight = 0;
        for (const auto& idx : sorted) {
            if (current_weight + idx.size_kb <= budget_kb) {
                current_weight += idx.size_kb;
                res.total_benefit += idx.benefit;
                res.selected_index_ids.push_back(idx.id);
                res.selected_index_names.push_back(idx.name);
            }
        }
        res.total_size_kb = current_weight;

        auto t_end = std::chrono::high_resolution_clock::now();
        res.runtime_us = std::chrono::duration<double, std::micro>(t_end - t_start).count();
        return res;
    }

    // -------------------------------------------------------------------------
    // 2. DYNAMIC PROGRAMMING (DP) SOLVER
    // Exact 0/1 Knapsack with table memoization and backtracking.
    // Handles discrete integer storage budget in KB.
    // Complexity: O(n * B) time, O(n * B) space
    // -------------------------------------------------------------------------
    SelectionResult solve_dp(int budget_kb) const {
        auto t_start = std::chrono::high_resolution_clock::now();

        int n = static_cast<int>(candidates.size());
        SelectionResult res;
        res.method_name = "Dynamic Programming (Exact)";
        res.budget_kb = budget_kb;
        res.optimal = true;

        if (n == 0 || budget_kb <= 0) {
            res.total_benefit = 0.0;
            res.total_size_kb = 0;
            res.runtime_us = 0.0;
            return res;
        }

        // DP Table: dp[i][w] = max benefit using subset of first i items with budget w
        // 2D vector for precise path reconstruction
        std::vector<std::vector<double>> dp(n + 1, std::vector<double>(budget_kb + 1, 0.0));

        for (int i = 1; i <= n; ++i) {
            int wt = candidates[i - 1].size_kb;
            double val = candidates[i - 1].benefit;
            for (int w = 0; w <= budget_kb; ++w) {
                if (wt <= w) {
                    dp[i][w] = std::max(dp[i - 1][w], dp[i - 1][w - wt] + val);
                } else {
                    dp[i][w] = dp[i - 1][w];
                }
            }
        }

        res.total_benefit = dp[n][budget_kb];

        // Backtrack to find chosen indexes
        int w = budget_kb;
        int total_size = 0;
        for (int i = n; i >= 1; --i) {
            if (dp[i][w] != dp[i - 1][w]) {
                const auto& item = candidates[i - 1];
                res.selected_index_ids.push_back(item.id);
                res.selected_index_names.push_back(item.name);
                w -= item.size_kb;
                total_size += item.size_kb;
            }
        }
        std::reverse(res.selected_index_ids.begin(), res.selected_index_ids.end());
        std::reverse(res.selected_index_names.begin(), res.selected_index_names.end());
        res.total_size_kb = total_size;

        auto t_end = std::chrono::high_resolution_clock::now();
        res.runtime_us = std::chrono::duration<double, std::micro>(t_end - t_start).count();
        return res;
    }

    // -------------------------------------------------------------------------
    // 3. EXACT ILP / BRANCH-AND-BOUND SOLVER
    // Solves max sum(b_i * x_i) s.t. sum(s_i * x_i) <= B, x_i in {0, 1}
    // Uses Linear Programming Continuous Knapsack Relaxation (Dantzig Bound)
    // for aggressive branch pruning.
    // Complexity: Exact global optimum, worst-case O(2^n), practical O(n log n + visited_nodes)
    // -------------------------------------------------------------------------
    SelectionResult solve_ilp(int budget_kb) const {
        auto t_start = std::chrono::high_resolution_clock::now();

        SelectionResult res;
        res.method_name = "ILP / Branch-and-Bound (Exact)";
        res.budget_kb = budget_kb;
        res.optimal = true;

        int n = static_cast<int>(candidates.size());
        if (n == 0 || budget_kb <= 0) {
            res.total_benefit = 0.0;
            res.total_size_kb = 0;
            res.runtime_us = 0.0;
            return res;
        }

        // Sort items by density for optimal LP relaxation bounding
        struct IndexedItem {
            int original_idx;
            int id;
            std::string name;
            int size_kb;
            double benefit;
            double density;
        };

        std::vector<IndexedItem> items(n);
        for (int i = 0; i < n; ++i) {
            items[i] = {i, candidates[i].id, candidates[i].name, candidates[i].size_kb, candidates[i].benefit,
                        candidates[i].size_kb > 0 ? (candidates[i].benefit / candidates[i].size_kb) : 0.0};
        }

        std::sort(items.begin(), items.end(), [](const IndexedItem& a, const IndexedItem& b) {
            return a.density > b.density;
        });

        double best_val = 0.0;
        int best_weight = 0;
        std::vector<int> best_assignment(n, 0);
        std::vector<int> current_assignment(n, 0);

        // Continuous knapsack relaxation upper bound function
        auto compute_lp_bound = [&](int level, int curr_weight, double curr_val) -> double {
            if (curr_weight > budget_kb) return 0.0;
            double bound = curr_val;
            int rem_cap = budget_kb - curr_weight;

            for (int i = level; i < n; ++i) {
                if (items[i].size_kb <= rem_cap) {
                    rem_cap -= items[i].size_kb;
                    bound += items[i].benefit;
                } else {
                    // Fractional part of critical item
                    bound += items[i].benefit * (static_cast<double>(rem_cap) / items[i].size_kb);
                    break;
                }
            }
            return bound;
        };

        // Recursive DFS Branch and Bound
        auto bnb_search = [&](auto& self, int level, int curr_weight, double curr_val) -> void {
            if (curr_weight > budget_kb) return;

            if (curr_val > best_val) {
                best_val = curr_val;
                best_weight = curr_weight;
                best_assignment = current_assignment;
            }

            if (level >= n) return;

            // Check LP relaxation upper bound
            double bound = compute_lp_bound(level, curr_weight, curr_val);
            if (bound <= best_val + 1e-9) {
                return; // Prune branch
            }

            // Branch 1: Include item (if it fits)
            if (curr_weight + items[level].size_kb <= budget_kb) {
                current_assignment[level] = 1;
                self(self, level + 1, curr_weight + items[level].size_kb, curr_val + items[level].benefit);
                current_assignment[level] = 0;
            }

            // Branch 0: Exclude item
            self(self, level + 1, curr_weight, curr_val);
        };

        bnb_search(bnb_search, 0, 0, 0.0);

        res.total_benefit = best_val;
        res.total_size_kb = best_weight;

        for (int i = 0; i < n; ++i) {
            if (best_assignment[i]) {
                res.selected_index_ids.push_back(items[i].id);
                res.selected_index_names.push_back(items[i].name);
            }
        }

        auto t_end = std::chrono::high_resolution_clock::now();
        res.runtime_us = std::chrono::duration<double, std::micro>(t_end - t_start).count();
        return res;
    }
};

} // namespace dbms
