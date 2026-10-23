/**
 * @file qubo.hpp
 * @brief Quadratic Unconstrained Binary Optimization (QUBO) and Ising Model in C++17.
 *
 * Implements:
 *  1. QUBO matrix representation: min_{x in {0,1}^n} x^T * Q * x.
 *  2. Explicit Binary-to-Spin conversion: x_i = (1 + s_i) / 2, with s_i in {-1, +1}.
 *  3. Exact conversion to Ising Hamiltonian: H(s) = sum_{i<j} J_ij * s_i * s_j + sum_i h_i * s_i + offset.
 *  4. Knapsack constraint penalty term encoding via binary slack expansion:
 *     min -sum v_i x_i + P * (sum w_i x_i + sum 2^k y_k - W)^2.
 *  5. Classical QUBO Solvers:
 *     - Brute-force exhaustive enumeration (exact global optimum for small n).
 *     - Simulated Annealing with exponential cooling schedule and deterministic seed.
 *
 * Optimization Techniques | Lab Assignment 8 (Track B)
 */

#ifndef QUBO_HPP
#define QUBO_HPP

#include <vector>
#include <cmath>
#include <stdexcept>
#include <string>
#include <sstream>
#include <random>
#include <chrono>
#include <algorithm>
#include <iostream>

namespace opt {

/**
 * @brief Ising model parameters: H(s) = sum_{i<j} J_ij * s_i * s_j + sum_i h_i * s_i + offset
 * with s_i in {-1, +1}.
 */
struct IsingModel {
    size_t num_spins;
    std::vector<std::vector<double>> J; // Couplings (upper triangular i < j)
    std::vector<double> h;              // Local magnetic fields
    double offset = 0.0;                // Constant energy shift

    explicit IsingModel(size_t n = 0)
        : num_spins(n), J(n, std::vector<double>(n, 0.0)), h(n, 0.0), offset(0.0) {}

    double energy(const std::vector<int>& spins) const {
        if (spins.size() != num_spins) {
            throw std::invalid_argument("IsingModel::energy: spin vector size mismatch.");
        }
        double e = offset;
        for (size_t i = 0; i < num_spins; ++i) {
            e += h[i] * spins[i];
            for (size_t j = i + 1; j < num_spins; ++j) {
                e += J[i][j] * spins[i] * spins[j];
            }
        }
        return e;
    }
};

/**
 * @brief QUBO formulation: E(x) = x^T * Q * x = sum_{i} Q_ii * x_i + sum_{i < j} (Q_ij + Q_ji) * x_i * x_j
 * with x_i in {0, 1}.
 */
class QUBO {
public:
    explicit QUBO(size_t n = 0)
        : n_(n), Q_(n, std::vector<double>(n, 0.0)) {}

    size_t size() const noexcept { return n_; }

    void resize(size_t n) {
        n_ = n;
        Q_.assign(n, std::vector<double>(n, 0.0));
    }

    double& at(size_t i, size_t j) {
        validate_index(i);
        validate_index(j);
        return Q_[i][j];
    }

    double at(size_t i, size_t j) const {
        validate_index(i);
        validate_index(j);
        return Q_[i][j];
    }

    const std::vector<std::vector<double>>& matrix() const noexcept { return Q_; }

    /**
     * @brief Evaluates energy E(x) = x^T * Q * x for binary vector x in {0, 1}^n.
     */
    double evaluate(const std::vector<int>& x) const {
        if (x.size() != n_) {
            throw std::invalid_argument("QUBO::evaluate: binary vector dimension mismatch.");
        }
        double val = 0.0;
        for (size_t i = 0; i < n_; ++i) {
            if (x[i] != 0 && x[i] != 1) {
                throw std::invalid_argument("QUBO::evaluate: variable must be binary 0 or 1.");
            }
            if (x[i] == 1) {
                for (size_t j = 0; j < n_; ++j) {
                    if (x[j] == 1) {
                        val += Q_[i][j];
                    }
                }
            }
        }
        return val;
    }

    /**
     * @brief Explicit binary-to-spin conversion mapping x_i = (1 + s_i) / 2 where s_i in {-1, +1}.
     *
     * Derivation:
     *   x_i * x_j = [(1 + s_i)/2] * [(1 + s_j)/2] = (1 + s_i + s_j + s_i * s_j) / 4
     *   x_i^2 = x_i = (1 + s_i) / 2
     *
     *   x^T * Q * x = sum_i Q_ii * [(1 + s_i)/2] + sum_{i < j} (Q_ij + Q_ji) * [(1 + s_i + s_j + s_i*s_j)/4]
     *               = sum_{i < j} J_ij * s_i * s_j + sum_i h_i * s_i + offset
     *
     *   where:
     *     J_ij   = (Q_ij + Q_ji) / 4
     *     h_i    = Q_ii / 2 + sum_{j != i} (Q_ij + Q_ji) / 4
     *     offset = sum_i Q_ii / 2 + sum_{i < j} (Q_ij + Q_ji) / 4
     */
    IsingModel to_ising() const {
        IsingModel ising(n_);

        for (size_t i = 0; i < n_; ++i) {
            // Diagonal linear contribution: Q_ii * (1 + s_i) / 2
            ising.h[i] += Q_[i][i] / 2.0;
            ising.offset += Q_[i][i] / 2.0;

            for (size_t j = i + 1; j < n_; ++j) {
                double q_sym = Q_[i][j] + Q_[j][i];

                // Quadratic coupling: (q_sym / 4) * s_i * s_j
                ising.J[i][j] = q_sym / 4.0;

                // Linear field contributions: (q_sym / 4) * s_i + (q_sym / 4) * s_j
                ising.h[i] += q_sym / 4.0;
                ising.h[j] += q_sym / 4.0;

                // Constant energy shift
                ising.offset += q_sym / 4.0;
            }
        }

        return ising;
    }

    /**
     * @brief Encodes 0/1 Knapsack problem into a QUBO using quadratic penalty terms.
     *
     * Formulation:
     *   min -sum_{i=1}^n v_i * x_i + P * (sum_{i=1}^n w_i * x_i + sum_{k=0}^{K-1} 2^k * y_k - W)^2
     *
     * where y_k are binary slack variables such that sum_{k=0}^{K-1} 2^k * y_k can represent
     * any integer in [0, W].
     */
    static QUBO from_knapsack(
        const std::vector<double>& values,
        const std::vector<double>& weights,
        double capacity,
        double penalty_factor = -1.0) {
        const size_t n = values.size();
        if (n != weights.size() || n == 0 || capacity <= 0.0) {
            throw std::invalid_argument("QUBO::from_knapsack: invalid input parameters.");
        }

        // Determine penalty multiplier P > max(v_i) to strictly penalize constraint violation
        double max_v = *std::max_element(values.begin(), values.end());
        double P = (penalty_factor > 0.0) ? penalty_factor : (2.0 * max_v);

        // Determine number of slack variables K: 2^K - 1 >= capacity
        size_t K = 1;
        while ((static_cast<double>((1ULL << K) - 1)) < capacity) {
            ++K;
        }

        const size_t total_vars = n + K;
        QUBO qubo(total_vars);

        // Combined coefficient vector c for: sum_{m} c_m * z_m - W
        // For m in [0, n-1]: c_m = w_m
        // For m = n + k:     c_m = 2^k
        std::vector<double> c(total_vars);
        for (size_t i = 0; i < n; ++i) c[i] = weights[i];
        for (size_t k = 0; k < K; ++k) c[n + k] = static_cast<double>(1ULL << k);

        // Expansion of P * (sum c_m z_m - W)^2:
        // = P * [ sum_m (c_m^2 - 2 * W * c_m) * z_m + 2 * sum_{m < l} c_m * c_l * z_m * z_l + W^2 ]
        for (size_t m = 0; m < total_vars; ++m) {
            qubo.at(m, m) += P * (c[m] * c[m] - 2.0 * capacity * c[m]);
            if (m < n) {
                // Objective maximization of value -> minimize -value
                qubo.at(m, m) -= values[m];
            }

            for (size_t l = m + 1; l < total_vars; ++l) {
                double cross_term = 2.0 * P * c[m] * c[l];
                qubo.at(m, l) += cross_term;
            }
        }

        return qubo;
    }

private:
    size_t n_;
    std::vector<std::vector<double>> Q_;

    void validate_index(size_t i) const {
        if (i >= n_) {
            throw std::out_of_range("QUBO index out of range: " + std::to_string(i));
        }
    }
};

/**
 * @brief Classical QUBO Solvers: Exhaustive Brute-Force & Simulated Annealing.
 */
class QUBOSolver {
public:
    struct Result {
        std::vector<int> best_bitstring;
        double best_energy = 0.0;
        double runtime_us = 0.0;
    };

    /**
     * @brief Exhaustive brute force over all 2^n configurations.
     */
    static Result solve_brute_force(const QUBO& qubo) {
        const size_t n = qubo.size();
        if (n == 0) throw std::invalid_argument("solve_brute_force: empty QUBO.");
        if (n > 24) {
            throw std::invalid_argument("solve_brute_force: n > 24 is too large for exhaustive search.");
        }

        auto start = std::chrono::high_resolution_clock::now();

        const uint64_t total_configs = 1ULL << n;
        double min_energy = std::numeric_limits<double>::infinity();
        std::vector<int> best_bits(n, 0);

        for (uint64_t cfg = 0; cfg < total_configs; ++cfg) {
            std::vector<int> x(n);
            for (size_t i = 0; i < n; ++i) {
                x[i] = (cfg >> i) & 1ULL;
            }
            double energy = qubo.evaluate(x);
            if (energy < min_energy) {
                min_energy = energy;
                best_bits = x;
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        Result res;
        res.best_bitstring = std::move(best_bits);
        res.best_energy = min_energy;
        res.runtime_us = static_cast<double>(
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
        return res;
    }

    /**
     * @brief Simulated Annealing with Metropolis-Hastings acceptance criterion.
     */
    static Result solve_simulated_annealing(
        const QUBO& qubo,
        size_t sweeps = 2000,
        double t_initial = 10.0,
        double t_final = 0.01,
        unsigned int seed = 42) {
        const size_t n = qubo.size();
        if (n == 0) throw std::invalid_argument("solve_simulated_annealing: empty QUBO.");

        auto start = std::chrono::high_resolution_clock::now();

        std::mt19937 gen(seed);
        std::uniform_int_distribution<int> bit_dist(0, 1);
        std::uniform_int_distribution<size_t> idx_dist(0, n - 1);
        std::uniform_real_distribution<double> uniform01(0.0, 1.0);

        // Random initial state
        std::vector<int> current_state(n);
        for (size_t i = 0; i < n; ++i) current_state[i] = bit_dist(gen);

        double current_energy = qubo.evaluate(current_state);
        std::vector<int> best_state = current_state;
        double best_energy = current_energy;

        const double cooling_rate = std::pow(t_final / t_initial, 1.0 / static_cast<double>(sweeps));
        double temp = t_initial;

        for (size_t step = 0; step < sweeps; ++step) {
            for (size_t rep = 0; rep < n; ++rep) {
                size_t flip_idx = idx_dist(gen);
                current_state[flip_idx] = 1 - current_state[flip_idx]; // Flip bit

                double new_energy = qubo.evaluate(current_state);
                double delta_e = new_energy - current_energy;

                // Metropolis criterion
                if (delta_e < 0.0 || uniform01(gen) < std::exp(-delta_e / temp)) {
                    current_energy = new_energy;
                    if (current_energy < best_energy) {
                        best_energy = current_energy;
                        best_state = current_state;
                    }
                } else {
                    // Reject flip
                    current_state[flip_idx] = 1 - current_state[flip_idx];
                }
            }
            temp *= cooling_rate;
        }

        auto end = std::chrono::high_resolution_clock::now();
        Result res;
        res.best_bitstring = std::move(best_state);
        res.best_energy = best_energy;
        res.runtime_us = static_cast<double>(
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
        return res;
    }
};

} // namespace opt

#endif // QUBO_HPP
