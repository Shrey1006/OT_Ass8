#ifndef MAX_COVERAGE_HPP
#define MAX_COVERAGE_HPP

#include <vector>
#include <string>
#include <set>
#include <random>
#include <algorithm>
#include <cmath>
#include <cassert>
#include <chrono>
#include <iostream>
#include <stdexcept>

namespace opt {

/**
 * @brief Representation of a Maximum Coverage Problem instance.
 *
 * Universe: U = {0, 1, ..., universe_size - 1}
 * Subsets:  S_0, S_1, ..., S_{num_subsets - 1}
 * Element weights: w(e) >= 0 for all e in U (defaults to unit weight 1.0)
 */
struct MaxCoverageInstance {
    size_t universe_size = 0;
    std::vector<std::vector<size_t>> subsets;
    std::vector<double> element_weights;

    /**
     * @brief Generates reproducible random maximum coverage instances.
     * @param n Number of subsets.
     * @param m Universe size.
     * @param p Probability that each element is included in each subset.
     * @param seed Deterministic random seed.
     */
    static MaxCoverageInstance generate_random(size_t n, size_t m, double p = 0.25, unsigned int seed = 42) {
        if (n == 0 || m == 0) {
            throw std::invalid_argument("MaxCoverageInstance: n and m must be > 0.");
        }
        if (p <= 0.0 || p > 1.0) {
            throw std::invalid_argument("MaxCoverageInstance: inclusion probability p must be in (0, 1].");
        }

        std::mt19937 rng(seed);
        std::bernoulli_distribution bern(p);

        MaxCoverageInstance inst;
        inst.universe_size = m;
        inst.element_weights.assign(m, 1.0); // Unit weights
        inst.subsets.resize(n);

        for (size_t i = 0; i < n; ++i) {
            for (size_t e = 0; e < m; ++e) {
                if (bern(rng)) {
                    inst.subsets[i].push_back(e);
                }
            }
            // Ensure every subset has at least one element to avoid trivial empty sets
            if (inst.subsets[i].empty()) {
                std::uniform_int_distribution<size_t> elem_dist(0, m - 1);
                inst.subsets[i].push_back(elem_dist(rng));
            }
            std::sort(inst.subsets[i].begin(), inst.subsets[i].end());
        }

        return inst;
    }
};

struct CoverageResult {
    std::string method;
    double coverage_value = 0.0;
    std::vector<size_t> selected_subsets;
    size_t covered_element_count = 0;
    double runtime_us = 0.0;
};

/**
 * @brief Submodular Maximum Coverage Solver providing Greedy and Exact Brute-Force solvers.
 *
 * THEORETICAL ASSUMPTIONS BEHIND THE (1 - 1/e) GREEDY GUARANTEE:
 * -------------------------------------------------------------------------------------
 * Nemhauser, Wolsey, and Fisher (1978) proved that Greedy achieves an approximation ratio
 * of (1 - 1/e) ~ 0.63212 under the following exact assumptions:
 *
 * 1. Monotonicity:
 *    For all A subseteq B subseteq S, f(A) <= f(B). Adding subsets never reduces coverage.
 *
 * 2. Submodularity (Diminishing Marginal Returns):
 *    For all A subseteq B subseteq S and e not in B:
 *        f(A cup {e}) - f(A) >= f(B cup {e}) - f(B).
 *    Any element covered by subset e provides at least as much new coverage to smaller
 *    set A as it does to superset B.
 *
 * 3. Normalization:
 *    f(emptyset) = 0. The empty selection covers zero elements.
 *
 * 4. Uniform Matroid / Cardinality Constraint:
 *    The selection constraint is strictly |A| <= k with uniform unit costs.
 *    (For non-uniform subset costs, simple greedy fails to achieve (1 - 1/e); cost-benefit
 *    greedy or partial enumeration is required).
 *
 * 5. Exact Evaluation Oracle:
 *    Marginal gains Delta(e | A) can be computed exactly without measurement noise.
 */
class MaxCoverageSolver {
public:
    /**
     * @brief Computes coverage function f(A) = weight of elements in union_{i in A} S_i.
     */
    static double evaluate_coverage(const MaxCoverageInstance& inst, const std::vector<size_t>& subset_indices) {
        if (subset_indices.empty()) return 0.0;

        std::vector<bool> covered(inst.universe_size, false);
        double total_val = 0.0;

        for (size_t idx : subset_indices) {
            if (idx >= inst.subsets.size()) {
                throw std::out_of_range("evaluate_coverage: subset index out of range.");
            }
            for (size_t elem : inst.subsets[idx]) {
                if (!covered[elem]) {
                    covered[elem] = true;
                    total_val += inst.element_weights[elem];
                }
            }
        }
        return total_val;
    }

    /**
     * @brief Evaluates marginal gain Delta(e | A) = f(A cup {e}) - f(A).
     */
    static double marginal_gain(const MaxCoverageInstance& inst,
                                const std::vector<size_t>& current_subsets,
                                size_t candidate_subset_idx) {
        std::vector<bool> covered(inst.universe_size, false);
        for (size_t idx : current_subsets) {
            for (size_t elem : inst.subsets[idx]) {
                covered[elem] = true;
            }
        }

        double gain = 0.0;
        for (size_t elem : inst.subsets[candidate_subset_idx]) {
            if (!covered[elem]) {
                gain += inst.element_weights[elem];
            }
        }
        return gain;
    }

    /**
     * @brief Classical Greedy Max-Coverage Algorithm.
     * Iteratively selects the subset with maximum marginal gain until k subsets are chosen.
     */
    static CoverageResult solve_greedy(const MaxCoverageInstance& inst, size_t k) {
        auto t_start = std::chrono::steady_clock::now();

        const size_t n = inst.subsets.size();
        if (k == 0) {
            return {"Greedy", 0.0, {}, 0, 0.0};
        }
        k = std::min(k, n);

        std::vector<size_t> selected;
        std::vector<bool> is_selected(n, false);
        std::vector<bool> element_covered(inst.universe_size, false);

        double total_cov = 0.0;
        size_t covered_count = 0;

        for (size_t step = 0; step < k; ++step) {
            size_t best_idx = n;
            double best_gain = -1.0;

            for (size_t i = 0; i < n; ++i) {
                if (is_selected[i]) continue;

                double gain = 0.0;
                for (size_t elem : inst.subsets[i]) {
                    if (!element_covered[elem]) {
                        gain += inst.element_weights[elem];
                    }
                }

                if (gain > best_gain) {
                    best_gain = gain;
                    best_idx = i;
                }
            }

            // If best gain is 0, all available elements are already covered
            if (best_idx == n || best_gain <= 0.0) {
                break;
            }

            is_selected[best_idx] = true;
            selected.push_back(best_idx);
            total_cov += best_gain;

            for (size_t elem : inst.subsets[best_idx]) {
                if (!element_covered[elem]) {
                    element_covered[elem] = true;
                    covered_count++;
                }
            }
        }

        auto t_end = std::chrono::steady_clock::now();
        double runtime_us = static_cast<double>(
            std::chrono::duration_cast<std::chrono::microseconds>(t_end - t_start).count());

        return {"Greedy (Marginal)", total_cov, selected, covered_count, runtime_us};
    }

    /**
     * @brief Exact Brute-Force Solver: enumerates all C(n, k) combinations.
     * Intended for small n (n <= 25).
     */
    static CoverageResult solve_brute_force(const MaxCoverageInstance& inst, size_t k) {
        auto t_start = std::chrono::steady_clock::now();

        const size_t n = inst.subsets.size();
        if (k == 0 || n == 0) {
            return {"Brute Force", 0.0, {}, 0, 0.0};
        }
        k = std::min(k, n);

        double best_val = -1.0;
        std::vector<size_t> best_selection;

        // Bitmask combination generator for C(n, k)
        std::vector<bool> mask(n, false);
        std::fill(mask.begin(), mask.begin() + k, true);

        do {
            std::vector<size_t> current_selection;
            for (size_t i = 0; i < n; ++i) {
                if (mask[i]) {
                    current_selection.push_back(i);
                }
            }

            double val = evaluate_coverage(inst, current_selection);
            if (val > best_val) {
                best_val = val;
                best_selection = current_selection;
            }
        } while (std::prev_permutation(mask.begin(), mask.end()));

        // Count distinct covered elements
        std::vector<bool> cov(inst.universe_size, false);
        size_t count = 0;
        for (size_t idx : best_selection) {
            for (size_t elem : inst.subsets[idx]) {
                if (!cov[elem]) {
                    cov[elem] = true;
                    count++;
                }
            }
        }

        auto t_end = std::chrono::steady_clock::now();
        double runtime_us = static_cast<double>(
            std::chrono::duration_cast<std::chrono::microseconds>(t_end - t_start).count());

        return {"Brute Force Exact", best_val, best_selection, count, runtime_us};
    }

    /**
     * @brief Numerically verifies the submodularity diminishing returns condition:
     *        For A subseteq B and e not in B:
     *            f(A cup {e}) - f(A) >= f(B cup {e}) - f(B)
     * @param inst The Max-Coverage problem instance.
     * @param num_pairs Number of random pairs (A, B) to test.
     * @param seed Random seed for sampling.
     * @return Pair of (passed_count, violation_count).
     */
    static std::pair<size_t, size_t> test_submodularity_property(const MaxCoverageInstance& inst,
                                                                 size_t num_pairs = 1000,
                                                                 unsigned int seed = 42) {
        const size_t n = inst.subsets.size();
        if (n < 2) return {0, 0};

        std::mt19937 rng(seed);
        size_t passed = 0;
        size_t violations = 0;

        for (size_t iter = 0; iter < num_pairs; ++iter) {
            // Pick a candidate element e in {0, ..., n-1}
            std::uniform_int_distribution<size_t> dist_n(0, n - 1);
            size_t e = dist_n(rng);

            // Construct random superset B from remaining elements
            std::vector<size_t> remaining;
            for (size_t i = 0; i < n; ++i) {
                if (i != e) remaining.push_back(i);
            }
            std::shuffle(remaining.begin(), remaining.end(), rng);

            std::uniform_int_distribution<size_t> size_dist(1, remaining.size());
            size_t size_b = size_dist(rng);

            std::vector<size_t> B(remaining.begin(), remaining.begin() + size_b);

            // Construct random subset A subseteq B
            std::uniform_int_distribution<size_t> size_a_dist(0, size_b);
            size_t size_a = size_a_dist(rng);

            std::vector<size_t> A(B.begin(), B.begin() + size_a);

            // Compute marginal gains
            double gain_A = marginal_gain(inst, A, e);
            double gain_B = marginal_gain(inst, B, e);

            // Diminishing returns: gain_A >= gain_B
            if (gain_A + 1e-12 >= gain_B) {
                passed++;
            } else {
                violations++;
            }
        }

        return {passed, violations};
    }
};

} // namespace opt

#endif // MAX_COVERAGE_HPP
