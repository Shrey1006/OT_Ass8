/**
 * @file knapsack.hpp
 * @brief 0/1 Knapsack Problem Solvers in C++17.
 *
 * Implements:
 *  1. Greedy heuristic (value-to-weight ratio ordering with 1/2-approximation guard).
 *  2. Dynamic Programming (exact O(N * W) optimal solution with backtracking).
 *  3. Branch-and-Bound with Dantzig fractional LP relaxation upper bound.
 *  4. Integer Linear Programming (ILP) formulation solved via Google OR-Tools MPSolver.
 *     (Includes fallback exact LP-relaxation Branch-and-Bound when OR-Tools is absent).
 *  5. Deterministic random instance generator for sizes N in {10, 20, 50, 100, 500}.
 *
 * Optimization Techniques | Lab Assignment 8 (Track B)
 */

#ifndef KNAPSACK_HPP
#define KNAPSACK_HPP

#include <vector>
#include <string>
#include <cstddef>
#include <algorithm>
#include <stdexcept>
#include <numeric>
#include <random>
#include <queue>
#include <chrono>
#include <iostream>
#include <cmath>

#if __has_include(<ortools/linear_solver/linear_solver.h>)
#define HAS_ORTOOLS 1
#include <ortools/linear_solver/linear_solver.h>
#else
#define HAS_ORTOOLS 0
#endif

namespace opt {

struct KnapsackItem {
    size_t id = 0;
    double value = 0.0;
    double weight = 0.0;
};

struct KnapsackInstance {
    std::vector<KnapsackItem> items;
    double capacity = 0.0;

    /**
     * @brief Generates reproducible random 0/1 knapsack instances.
     * @param n Number of items.
     * @param capacity_ratio Ratio of total item weight to set capacity (default 0.5).
     * @param seed Fixed deterministic random seed.
     */
    static KnapsackInstance generate_random(size_t n, double capacity_ratio = 0.5, unsigned int seed = 42) {
        if (n == 0) {
            throw std::invalid_argument("KnapsackInstance: item count n must be > 0.");
        }
        if (capacity_ratio <= 0.0 || capacity_ratio >= 1.0) {
            throw std::invalid_argument("KnapsackInstance: capacity_ratio must be in (0, 1).");
        }

        std::mt19937 gen(seed);
        std::uniform_real_distribution<double> val_dist(10.0, 100.0);
        std::uniform_real_distribution<double> wt_dist(5.0, 50.0);

        KnapsackInstance inst;
        inst.items.resize(n);
        double total_weight = 0.0;

        for (size_t i = 0; i < n; ++i) {
            inst.items[i].id = i;
            inst.items[i].value = std::round(val_dist(gen));
            inst.items[i].weight = std::round(wt_dist(gen));
            total_weight += inst.items[i].weight;
        }

        inst.capacity = std::round(total_weight * capacity_ratio);
        return inst;
    }
};

struct KnapsackResult {
    std::string method;
    double total_value = 0.0;
    double total_weight = 0.0;
    std::vector<bool> selected;
    double runtime_us = 0.0;
    bool is_exact = false;
};

class KnapsackSolver {
public:
    /**
     * @brief Greedy heuristic: sorts items by density (value / weight).
     * Takes items greedily while capacity permits.
     * Guaranteed 1/2-approximation by taking max(greedy_pack, best_single_item).
     */
    static KnapsackResult solve_greedy(const KnapsackInstance& inst) {
        validate_instance(inst);
        auto start = std::chrono::high_resolution_clock::now();

        const size_t n = inst.items.size();
        std::vector<size_t> order(n);
        std::iota(order.begin(), order.end(), 0);

        // Sort descending by ratio value / weight
        std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
            double r_a = inst.items[a].value / inst.items[a].weight;
            double r_b = inst.items[b].value / inst.items[b].weight;
            return r_a > r_b;
        });

        std::vector<bool> selected(n, false);
        double current_val = 0.0;
        double current_wt = 0.0;

        for (size_t idx : order) {
            if (current_wt + inst.items[idx].weight <= inst.capacity) {
                selected[idx] = true;
                current_wt += inst.items[idx].weight;
                current_val += inst.items[idx].value;
            }
        }

        // Compare with best single item fitting knapsack (standard 1/2 approx enhancement)
        size_t best_single = n;
        double best_single_val = 0.0;
        for (size_t i = 0; i < n; ++i) {
            if (inst.items[i].weight <= inst.capacity && inst.items[i].value > best_single_val) {
                best_single_val = inst.items[i].value;
                best_single = i;
            }
        }

        if (best_single_val > current_val && best_single < n) {
            std::fill(selected.begin(), selected.end(), false);
            selected[best_single] = true;
            current_val = best_single_val;
            current_wt = inst.items[best_single].weight;
        }

        auto end = std::chrono::high_resolution_clock::now();
        KnapsackResult res;
        res.method = "Greedy (Value/Weight)";
        res.total_value = current_val;
        res.total_weight = current_wt;
        res.selected = std::move(selected);
        res.runtime_us = static_cast<double>(
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
        res.is_exact = false;
        return res;
    }

    /**
     * @brief Dynamic Programming: exact pseudo-polynomial O(N * W) solver.
     */
    static KnapsackResult solve_dynamic_programming(const KnapsackInstance& inst) {
        validate_instance(inst);
        auto start = std::chrono::high_resolution_clock::now();

        const size_t n = inst.items.size();
        const size_t W = static_cast<size_t>(inst.capacity);

        // dp[i][w]: optimal value considering first i items with weight capacity w
        std::vector<std::vector<double>> dp(n + 1, std::vector<double>(W + 1, 0.0));

        for (size_t i = 1; i <= n; ++i) {
            const auto& item = inst.items[i - 1];
            size_t w_item = static_cast<size_t>(item.weight);
            double v_item = item.value;

            for (size_t w = 0; w <= W; ++w) {
                dp[i][w] = dp[i - 1][w];
                if (w >= w_item) {
                    dp[i][w] = std::max(dp[i][w], dp[i - 1][w - w_item] + v_item);
                }
            }
        }

        // Backtracking to find selected items
        std::vector<bool> selected(n, false);
        size_t rem_w = W;
        for (size_t i = n; i >= 1; --i) {
            if (dp[i][rem_w] != dp[i - 1][rem_w]) {
                selected[i - 1] = true;
                rem_w -= static_cast<size_t>(inst.items[i - 1].weight);
            }
        }

        double total_wt = 0.0;
        for (size_t i = 0; i < n; ++i) {
            if (selected[i]) total_wt += inst.items[i].weight;
        }

        auto end = std::chrono::high_resolution_clock::now();
        KnapsackResult res;
        res.method = "Dynamic Programming";
        res.total_value = dp[n][W];
        res.total_weight = total_wt;
        res.selected = std::move(selected);
        res.runtime_us = static_cast<double>(
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
        res.is_exact = true;
        return res;
    }

    /**
     * @brief Branch-and-Bound with Dantzig fractional LP relaxation upper bound.
     */
    static KnapsackResult solve_branch_and_bound(const KnapsackInstance& inst) {
        validate_instance(inst);
        auto start = std::chrono::high_resolution_clock::now();

        const size_t n = inst.items.size();

        // Sort items by value/weight ratio descending
        struct IndexedItem {
            size_t orig_idx;
            double value;
            double weight;
            double ratio;
        };
        std::vector<IndexedItem> sorted_items(n);
        for (size_t i = 0; i < n; ++i) {
            sorted_items[i] = {i, inst.items[i].value, inst.items[i].weight,
                               inst.items[i].value / inst.items[i].weight};
        }
        std::sort(sorted_items.begin(), sorted_items.end(), [](const IndexedItem& a, const IndexedItem& b) {
            return a.ratio > b.ratio;
        });

        auto compute_bound = [&](size_t level, double cur_val, double cur_wt) -> double {
            if (cur_wt > inst.capacity) return 0.0;
            double bound = cur_val;
            double rem_cap = inst.capacity - cur_wt;

            for (size_t j = level; j < n; ++j) {
                if (sorted_items[j].weight <= rem_cap) {
                    rem_cap -= sorted_items[j].weight;
                    bound += sorted_items[j].value;
                } else {
                    // Fractional part of next item
                    bound += sorted_items[j].value * (rem_cap / sorted_items[j].weight);
                    break;
                }
            }
            return bound;
        };

        struct Node {
            size_t level;
            double val;
            double wt;
            double bound;
            std::vector<bool> taken;
        };

        struct NodeCompare {
            bool operator()(const Node& a, const Node& b) const {
                return a.bound < b.bound; // Max-heap based on upper bound
            }
        };

        std::priority_queue<Node, std::vector<Node>, NodeCompare> pq;

        Node root{0, 0.0, 0.0, compute_bound(0, 0.0, 0.0), std::vector<bool>(n, false)};
        pq.push(root);

        double best_val = 0.0;
        std::vector<bool> best_taken(n, false);

        while (!pq.empty()) {
            Node cur = pq.top();
            pq.pop();

            if (cur.bound <= best_val) continue;

            if (cur.level == n) {
                if (cur.val > best_val) {
                    best_val = cur.val;
                    best_taken = cur.taken;
                }
                continue;
            }

            const auto& next_item = sorted_items[cur.level];

            // Branch 1: INCLUDE next_item (if it fits)
            if (cur.wt + next_item.weight <= inst.capacity) {
                Node left = cur;
                left.level = cur.level + 1;
                left.wt += next_item.weight;
                left.val += next_item.value;
                left.taken[cur.level] = true;
                left.bound = compute_bound(left.level, left.val, left.wt);

                if (left.val > best_val) {
                    best_val = left.val;
                    best_taken = left.taken;
                }
                if (left.bound > best_val) {
                    pq.push(left);
                }
            }

            // Branch 2: EXCLUDE next_item
            Node right = cur;
            right.level = cur.level + 1;
            right.bound = compute_bound(right.level, right.val, right.wt);

            if (right.bound > best_val) {
                pq.push(right);
            }
        }

        // Remap to original item indices
        std::vector<bool> final_selected(n, false);
        double total_wt = 0.0;
        for (size_t i = 0; i < n; ++i) {
            if (best_taken[i]) {
                final_selected[sorted_items[i].orig_idx] = true;
                total_wt += sorted_items[i].weight;
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        KnapsackResult res;
        res.method = "Branch-and-Bound (Fractional Bound)";
        res.total_value = best_val;
        res.total_weight = total_wt;
        res.selected = std::move(final_selected);
        res.runtime_us = static_cast<double>(
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
        res.is_exact = true;
        return res;
    }

    /**
     * @brief ILP formulation solved via Google OR-Tools (or exact fallback).
     *
     * Mathematical Formulation:
     *   max   \sum_{i=1}^n v_i * x_i
     *   s.t.  \sum_{i=1}^n w_i * x_i <= W
     *         x_i \in {0, 1}
     */
    static KnapsackResult solve_ilp(const KnapsackInstance& inst) {
        validate_instance(inst);

#if HAS_ORTOOLS
        auto start = std::chrono::high_resolution_clock::now();
        const size_t n = inst.items.size();

        // Instantiate SCIP or CBC solver through MPSolver
        std::unique_ptr<operations_research::MPSolver> solver(
            operations_research::MPSolver::CreateSolver("SCIP"));
        if (!solver) {
            solver.reset(operations_research::MPSolver::CreateSolver("CBC"));
        }
        if (!solver) {
            throw std::runtime_error("Google OR-Tools: unable to instantiate SCIP or CBC solver backend.");
        }

        // Binary decision variables x_i in {0, 1}
        std::vector<const operations_research::MPVariable*> x(n);
        for (size_t i = 0; i < n; ++i) {
            x[i] = solver->MakeBoolVar("x_" + std::to_string(i));
        }

        // Knapsack weight constraint: \sum w_i * x_i <= W
        operations_research::MPConstraint* const capacity_constraint =
            solver->MakeRowConstraint(0.0, inst.capacity, "capacity");
        for (size_t i = 0; i < n; ++i) {
            capacity_constraint->SetCoefficient(x[i], inst.items[i].weight);
        }

        // Objective: maximize \sum v_i * x_i
        operations_research::MPObjective* const objective = solver->MutableObjective();
        for (size_t i = 0; i < n; ++i) {
            objective->SetCoefficient(x[i], inst.items[i].value);
        }
        objective->SetMaximization();

        // Solve ILP
        const operations_research::MPSolver::ResultStatus result_status = solver->Solve();
        if (result_status != operations_research::MPSolver::OPTIMAL) {
            throw std::runtime_error("Google OR-Tools ILP solver failed to find optimal solution.");
        }

        std::vector<bool> selected(n, false);
        double total_wt = 0.0;
        for (size_t i = 0; i < n; ++i) {
            if (x[i]->solution_value() > 0.5) {
                selected[i] = true;
                total_wt += inst.items[i].weight;
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        KnapsackResult res;
        res.method = "ILP (Google OR-Tools)";
        res.total_value = objective->Value();
        res.total_weight = total_wt;
        res.selected = std::move(selected);
        res.runtime_us = static_cast<double>(
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
        res.is_exact = true;
        return res;

#else
        // Fallback exact Branch-and-Bound LP relaxation when OR-Tools is not linked
        auto res = solve_branch_and_bound(inst);
        res.method = "ILP (Branch-and-Bound Formulation)";
        return res;
#endif
    }

private:
    static void validate_instance(const KnapsackInstance& inst) {
        if (inst.items.empty()) {
            throw std::invalid_argument("KnapsackSolver: instance item list is empty.");
        }
        if (inst.capacity <= 0.0) {
            throw std::invalid_argument("KnapsackSolver: capacity must be strictly positive.");
        }
        for (size_t i = 0; i < inst.items.size(); ++i) {
            if (inst.items[i].value < 0.0 || inst.items[i].weight <= 0.0) {
                throw std::invalid_argument("KnapsackSolver: item values must be >= 0 and weights > 0.");
            }
        }
    }
};

} // namespace opt

#endif // KNAPSACK_HPP
