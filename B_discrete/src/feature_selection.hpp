/**
 * @file feature_selection.hpp
 * @brief Feature Selection via mRMR (Minimum Redundancy Maximum Relevance) and QUBO.
 *
 * Implements:
 *  1. mRMR Feature Selection Formulation:
 *     max   \sum_{i=1}^n r_i * x_i - \lambda * \sum_{i < j} C_{ij} * x_i * x_j
 *     s.t.  \sum_{i=1}^n x_i = k,  x_i \in {0, 1}
 *  2. Greedy Forward Selection:
 *     Iteratively picks features maximizing marginal relevance minus redundancy to existing set.
 *  3. Quadratic Penalty QUBO Encoding:
 *     Encodes exact cardinality constraint (\sum x_i - k)^2 into diagonal and cross terms:
 *       Q_ii = -r_i + P * (1 - 2*k)
 *       Q_ij = \lambda * C_ij + 2 * P  (for i < j)
 *     Constant offset: P * k^2.
 *  4. Comparative evaluation: Greedy vs. QUBO Exact Brute-Force vs. Simulated Annealing.
 *
 * Optimization Techniques | Lab Assignment 8 (Track B)
 */

#ifndef FEATURE_SELECTION_HPP
#define FEATURE_SELECTION_HPP

#include "qubo.hpp"
#include <vector>
#include <cmath>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <random>
#include <chrono>
#include <iostream>

namespace opt {

struct FeatureSelectionResult {
    std::string method;
    std::vector<int> selected_features; // Indices of selected features
    double relevance_sum = 0.0;
    double redundancy_penalty = 0.0;
    double objective_score = 0.0; // relevance - lambda * redundancy
    double runtime_us = 0.0;
    bool is_feasible = false;
};

class FeatureSelectionProblem {
public:
    size_t n = 0;       // Total number of candidate features
    size_t k = 0;       // Number of features to select
    std::vector<double> relevance;              // Relevance score r_i >= 0
    std::vector<std::vector<double>> redundancy; // Cross-redundancy C_ij in [0, 1]
    double lambda = 1.0;                         // Trade-off weight between relevance and redundancy

    FeatureSelectionProblem() = default;

    FeatureSelectionProblem(
        size_t num_features,
        size_t select_k,
        std::vector<double> rel,
        std::vector<std::vector<double>> red,
        double red_weight = 1.0)
        : n(num_features), k(select_k), relevance(std::move(rel)),
          redundancy(std::move(red)), lambda(red_weight) {
        validate();
    }

    void validate() const {
        if (n == 0 || k == 0 || k > n) {
            throw std::invalid_argument("FeatureSelectionProblem: must satisfy 1 <= k <= n.");
        }
        if (relevance.size() != n) {
            throw std::invalid_argument("FeatureSelectionProblem: relevance vector size mismatch.");
        }
        if (redundancy.size() != n) {
            throw std::invalid_argument("FeatureSelectionProblem: redundancy matrix row size mismatch.");
        }
        for (size_t i = 0; i < n; ++i) {
            if (redundancy[i].size() != n) {
                throw std::invalid_argument("FeatureSelectionProblem: redundancy matrix must be square.");
            }
            if (relevance[i] < 0.0) {
                throw std::invalid_argument("FeatureSelectionProblem: relevance scores must be non-negative.");
            }
        }
    }

    /**
     * @brief Generates synthetic feature selection problem with controllable relevance and redundancy.
     */
    static FeatureSelectionProblem generate_synthetic(
        size_t n,
        size_t k,
        double lambda = 1.0,
        unsigned int seed = 42) {
        if (n == 0 || k == 0 || k > n) {
            throw std::invalid_argument("generate_synthetic: must satisfy 1 <= k <= n.");
        }

        std::mt19937 gen(seed);
        std::uniform_real_distribution<double> rel_dist(0.1, 1.0);
        std::uniform_real_distribution<double> red_dist(0.0, 0.8);

        std::vector<double> rel(n);
        for (size_t i = 0; i < n; ++i) rel[i] = rel_dist(gen);

        std::vector<std::vector<double>> red(n, std::vector<double>(n, 0.0));
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = i + 1; j < n; ++j) {
                double corr = red_dist(gen);
                red[i][j] = corr;
                red[j][i] = corr;
            }
        }

        return FeatureSelectionProblem(n, k, std::move(rel), std::move(red), lambda);
    }

    /**
     * @brief Computes mRMR objective value: J(x) = sum r_i x_i - lambda * sum_{i < j} C_ij x_i x_j.
     */
    double evaluate_score(const std::vector<int>& x) const {
        if (x.size() != n) {
            throw std::invalid_argument("evaluate_score: dimension mismatch.");
        }
        double rel_sum = 0.0;
        double red_sum = 0.0;

        for (size_t i = 0; i < n; ++i) {
            if (x[i] == 1) {
                rel_sum += relevance[i];
                for (size_t j = i + 1; j < n; ++j) {
                    if (x[j] == 1) {
                        red_sum += redundancy[i][j];
                    }
                }
            }
        }

        return rel_sum - lambda * red_sum;
    }

    /**
     * @brief Greedy Forward Selection algorithm:
     * Starts with feature of highest relevance, then iteratively adds feature maximizing marginal gain:
     *   gain(j) = r_j - lambda * sum_{i in S} C_ij
     */
    FeatureSelectionResult solve_greedy() const {
        auto start = std::chrono::high_resolution_clock::now();

        std::vector<int> selected_mask(n, 0);
        std::vector<int> chosen_indices;
        chosen_indices.reserve(k);

        // Step 1: select single feature with highest relevance
        size_t best_first = 0;
        double best_rel = -1.0;
        for (size_t i = 0; i < n; ++i) {
            if (relevance[i] > best_rel) {
                best_rel = relevance[i];
                best_first = i;
            }
        }
        selected_mask[best_first] = 1;
        chosen_indices.push_back(static_cast<int>(best_first));

        // Step 2 to k: forward selection
        for (size_t step = 1; step < k; ++step) {
            size_t best_cand = n;
            double best_gain = -std::numeric_limits<double>::infinity();

            for (size_t j = 0; j < n; ++j) {
                if (selected_mask[j] == 1) continue;

                // Marginal gain = r_j - lambda * sum_{i in chosen} C_ij
                double red_penalty = 0.0;
                for (int sel : chosen_indices) {
                    red_penalty += redundancy[j][sel];
                }
                double gain = relevance[j] - lambda * red_penalty;

                if (gain > best_gain) {
                    best_gain = gain;
                    best_cand = j;
                }
            }

            if (best_cand < n) {
                selected_mask[best_cand] = 1;
                chosen_indices.push_back(static_cast<int>(best_cand));
            }
        }

        auto end = std::chrono::high_resolution_clock::now();

        FeatureSelectionResult res;
        res.method = "Greedy Forward Selection";
        res.selected_features = chosen_indices;
        res.objective_score = evaluate_score(selected_mask);

        double rel_tot = 0.0;
        double red_tot = 0.0;
        for (size_t i = 0; i < n; ++i) {
            if (selected_mask[i]) {
                rel_tot += relevance[i];
                for (size_t j = i + 1; j < n; ++j) {
                    if (selected_mask[j]) red_tot += redundancy[i][j];
                }
            }
        }
        res.relevance_sum = rel_tot;
        res.redundancy_penalty = red_tot;
        res.is_feasible = (chosen_indices.size() == k);
        res.runtime_us = static_cast<double>(
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());

        return res;
    }

    /**
     * @brief Encodes the mRMR problem into a QUBO matrix Q with cardinality penalty P*(sum x_i - k)^2.
     *
     * Formulation:
     *   min -sum r_i x_i + lambda * sum_{i < j} C_ij x_i x_j + P * (sum x_i - k)^2
     *
     * Expansion of (sum x_i - k)^2:
     *   = (sum x_i)^2 - 2*k*(sum x_i) + k^2
     *   = sum x_i + 2 * sum_{i < j} x_i x_j - 2*k * sum x_i + k^2
     *   = (1 - 2*k) * sum x_i + 2 * sum_{i < j} x_i x_j + k^2
     *
     * QUBO coefficients:
     *   Q_ii = -r_i + P * (1 - 2 * k)
     *   Q_ij = lambda * C_ij + 2 * P  (for i < j)
     *   offset = P * k^2
     */
    std::pair<QUBO, double> to_qubo(double penalty_multiplier = -1.0) const {
        // Automatically determine penalty P: must exceed maximum single-swap gain
        double max_r = *std::max_element(relevance.begin(), relevance.end());
        double P = (penalty_multiplier > 0.0) ? penalty_multiplier : (4.0 * max_r + 5.0);

        QUBO qubo(n);

        // Diagonal terms
        for (size_t i = 0; i < n; ++i) {
            qubo.at(i, i) = -relevance[i] + P * (1.0 - 2.0 * static_cast<double>(k));
        }

        // Off-diagonal terms (i < j)
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = i + 1; j < n; ++j) {
                qubo.at(i, j) = lambda * redundancy[i][j] + 2.0 * P;
            }
        }

        double offset = P * static_cast<double>(k * k);
        return {qubo, offset};
    }

    /**
     * @brief Decodes a QUBO binary vector into a FeatureSelectionResult.
     */
    FeatureSelectionResult decode_result(
        const std::string& method_name,
        const std::vector<int>& bitstring,
        double runtime_us) const {
        FeatureSelectionResult res;
        res.method = method_name;
        res.runtime_us = runtime_us;

        size_t count = 0;
        double rel_tot = 0.0;
        double red_tot = 0.0;

        for (size_t i = 0; i < n; ++i) {
            if (bitstring[i] == 1) {
                res.selected_features.push_back(static_cast<int>(i));
                rel_tot += relevance[i];
                ++count;
                for (size_t j = i + 1; j < n; ++j) {
                    if (bitstring[j] == 1) {
                        red_tot += redundancy[i][j];
                    }
                }
            }
        }

        res.is_feasible = (count == k);
        res.relevance_sum = rel_tot;
        res.redundancy_penalty = red_tot;
        res.objective_score = rel_tot - lambda * red_tot;
        return res;
    }
};

} // namespace opt

#endif // FEATURE_SELECTION_HPP
