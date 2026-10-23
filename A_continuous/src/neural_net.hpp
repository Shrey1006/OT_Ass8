/**
 * @file neural_net.hpp
 * @brief Two-Layer Multi-Layer Perceptron (MLP) with hand-written backpropagation.
 *
 * ==============================================================================
 * CONCEPTUAL DISTINCTION: BACKPROPAGATION vs. THE OPTIMIZER
 * ==============================================================================
 * A fundamental concept in Chapter 6 optimization for machine learning is that
 * Backpropagation and Optimization are completely orthogonal, decoupled procedures:
 *
 * 1. BACKPROPAGATION (Gradient Computation Engine):
 *    - Backpropagation is an application of reverse-mode automatic differentiation
 *      (the multivariable calculus chain rule) on a computational directed acyclic graph.
 *    - Its sole responsibility is to compute the exact partial derivatives of a scalar
 *      loss function L with respect to all internal parameters \theta:
 *          g_t = \nabla_\theta L(\theta_t; X, y)
 *    - Backpropagation DOES NOT modify parameters.
 *    - Backpropagation has NO notion of learning rate (\eta), momentum (\beta),
 *      second moments, weight decay, line search, or convergence stopping criteria.
 *    - Its time complexity is O(|E| + |V|), scaling linearly with the number of
 *      operations in the forward pass.
 *
 * 2. THE OPTIMIZER (Parameter Trajectory Engine):
 *    - The optimizer is the search algorithm responsible for taking the gradient
 *      vector g_t provided by backpropagation (or any other gradient oracle,
 *      such as finite differences or parameter-shift in quantum circuits) and
 *      determining the displacement vector \Delta \theta:
 *          \theta_{t+1} = \theta_t + \Delta \theta(g_t, \text{state}_t)
 *    - The optimizer handles convergence rates, conditioning, momentum accumulation,
 *      adaptive coordinate scaling (AdaGrad, RMSProp, Adam), curvature corrections,
 *      stochastic noise mitigation, and stopping criteria.
 *    - The optimizer is agnostic to HOW the gradient was computed; it only consumes
 *      g_t as a directional signal.
 * ==============================================================================
 *
 * Optimization Techniques | Lab Assignment 8 (Track A)
 */

#ifndef NEURAL_NET_HPP
#define NEURAL_NET_HPP

#include <vector>
#include <cmath>
#include <random>
#include <stdexcept>
#include <string>
#include <sstream>
#include <cstddef>
#include <iostream>

namespace opt {

/**
 * @brief Two-Layer Multi-Layer Perceptron (Input -> Hidden -> Output).
 *
 * Architecture:
 *   z1 = W1 * x + b1       (Hidden pre-activation)
 *   a1 = tanh(z1)          (Hidden activation)
 *   z2 = W2 * a1 + b2      (Output layer / predictions)
 *   Loss = (1 / (2 * N)) * \sum_{i=1}^N ||z2^{(i)} - y^{(i)}||^2  (Mean Squared Error)
 */
class TwoLayerMLP {
public:
    TwoLayerMLP(size_t in_dim, size_t hidden_dim, size_t out_dim, unsigned int seed = 42)
        : in_dim_(in_dim), hidden_dim_(hidden_dim), out_dim_(out_dim) {
        if (in_dim_ == 0 || hidden_dim_ == 0 || out_dim_ == 0) {
            throw std::invalid_argument("TwoLayerMLP: layer dimensions must be strictly positive.");
        }

        initialize_parameters(seed);
    }

    size_t in_dim() const noexcept { return in_dim_; }
    size_t hidden_dim() const noexcept { return hidden_dim_; }
    size_t out_dim() const noexcept { return out_dim_; }

    size_t total_parameters() const noexcept {
        // W1: hidden_dim * in_dim, b1: hidden_dim
        // W2: out_dim * hidden_dim, b2: out_dim
        return (hidden_dim_ * in_dim_) + hidden_dim_ + (out_dim_ * hidden_dim_) + out_dim_;
    }

    /**
     * @brief Exports all network parameters into a single contiguous 1D vector.
     * Order: [W1 elements row-major, b1, W2 elements row-major, b2]
     */
    std::vector<double> get_parameters() const {
        std::vector<double> params;
        params.reserve(total_parameters());

        for (const auto& row : W1_) {
            for (double w : row) params.push_back(w);
        }
        for (double b : b1_) params.push_back(b);

        for (const auto& row : W2_) {
            for (double w : row) params.push_back(w);
        }
        for (double b : b2_) params.push_back(b);

        return params;
    }

    /**
     * @brief Ingests contiguous 1D parameter vector into internal weight matrices and biases.
     */
    void set_parameters(const std::vector<double>& params) {
        if (params.size() != total_parameters()) {
            std::ostringstream oss;
            oss << "TwoLayerMLP::set_parameters: expected " << total_parameters()
                << " parameters, got " << params.size();
            throw std::invalid_argument(oss.str());
        }

        size_t idx = 0;
        for (size_t i = 0; i < hidden_dim_; ++i) {
            for (size_t j = 0; j < in_dim_; ++j) {
                W1_[i][j] = params[idx++];
            }
        }
        for (size_t i = 0; i < hidden_dim_; ++i) {
            b1_[i] = params[idx++];
        }
        for (size_t i = 0; i < out_dim_; ++i) {
            for (size_t j = 0; j < hidden_dim_; ++j) {
                W2_[i][j] = params[idx++];
            }
        }
        for (size_t i = 0; i < out_dim_; ++i) {
            b2_[i] = params[idx++];
        }
    }

    /**
     * @brief Forward pass for a single input vector.
     * @param x Input feature vector of size in_dim.
     * @return Output prediction vector of size out_dim.
     */
    std::vector<double> forward(const std::vector<double>& x) const {
        if (x.size() != in_dim_) {
            throw std::invalid_argument("TwoLayerMLP::forward: input dimension mismatch.");
        }

        // Hidden layer: z1 = W1 * x + b1, a1 = tanh(z1)
        std::vector<double> a1(hidden_dim_);
        for (size_t i = 0; i < hidden_dim_; ++i) {
            double sum = b1_[i];
            for (size_t j = 0; j < in_dim_; ++j) {
                sum += W1_[i][j] * x[j];
            }
            a1[i] = std::tanh(sum);
        }

        // Output layer: y_hat = W2 * a1 + b2
        std::vector<double> y_hat(out_dim_);
        for (size_t i = 0; i < out_dim_; ++i) {
            double sum = b2_[i];
            for (size_t j = 0; j < hidden_dim_; ++j) {
                sum += W2_[i][j] * a1[j];
            }
            y_hat[i] = sum;
        }

        return y_hat;
    }

    /**
     * @brief Computes Mean Squared Error (MSE) loss over a batch of samples.
     *        Loss = (1 / (2 * N)) * \sum_{i=1}^N ||y_hat^{(i)} - y^{(i)}||^2
     */
    double compute_loss(
        const std::vector<std::vector<double>>& X,
        const std::vector<std::vector<double>>& Y) const {
        validate_dataset(X, Y);

        const size_t N = X.size();
        double total_sse = 0.0;

        for (size_t k = 0; k < N; ++k) {
            auto pred = forward(X[k]);
            for (size_t i = 0; i < out_dim_; ++i) {
                double diff = pred[i] - Y[k][i];
                total_sse += diff * diff;
            }
        }

        return total_sse / (2.0 * static_cast<double>(N));
    }

    /**
     * @brief Hand-written backpropagation calculating exact analytical gradients.
     *
     * Mathematical derivation via chain rule:
     *   For sample k:
     *     \partial L / \partial z2_i = (1 / N) * (z2_i - y_i)
     *     \partial L / \partial W2_ij = (\partial L / \partial z2_i) * a1_j
     *     \partial L / \partial b2_i = \partial L / \partial z2_i
     *
     *   Backpropagating to hidden layer:
     *     \partial L / \partial a1_j = \sum_i (\partial L / \partial z2_i) * W2_ij
     *     Since a1_j = tanh(z1_j), d(tanh(u))/du = 1 - tanh^2(u) = 1 - a1_j^2:
     *     \delta1_j = (\partial L / \partial z1_j) = (\partial L / \partial a1_j) * (1 - a1_j^2)
     *
     *     \partial L / \partial W1_jm = \delta1_j * x_m
     *     \partial L / \partial b1_j = \delta1_j
     *
     * @return 1D vector of accumulated gradients aligned with get_parameters().
     */
    std::vector<double> compute_gradients(
        const std::vector<std::vector<double>>& X,
        const std::vector<std::vector<double>>& Y) const {
        validate_dataset(X, Y);

        const size_t N = X.size();
        const double inv_N = 1.0 / static_cast<double>(N);

        std::vector<std::vector<double>> dW1(hidden_dim_, std::vector<double>(in_dim_, 0.0));
        std::vector<double> db1(hidden_dim_, 0.0);
        std::vector<std::vector<double>> dW2(out_dim_, std::vector<double>(hidden_dim_, 0.0));
        std::vector<double> db2(out_dim_, 0.0);

        for (size_t k = 0; k < N; ++k) {
            const auto& x = X[k];
            const auto& y = Y[k];

            // 1. Forward pass storing activations
            std::vector<double> a1(hidden_dim_);
            for (size_t i = 0; i < hidden_dim_; ++i) {
                double sum = b1_[i];
                for (size_t j = 0; j < in_dim_; ++j) {
                    sum += W1_[i][j] * x[j];
                }
                a1[i] = std::tanh(sum);
            }

            std::vector<double> z2(out_dim_);
            for (size_t i = 0; i < out_dim_; ++i) {
                double sum = b2_[i];
                for (size_t j = 0; j < hidden_dim_; ++j) {
                    sum += W2_[i][j] * a1[j];
                }
                z2[i] = sum;
            }

            // 2. Output error: dL/dz2 = (1/N) * (z2 - y)
            std::vector<double> delta2(out_dim_);
            for (size_t i = 0; i < out_dim_; ++i) {
                delta2[i] = inv_N * (z2[i] - y[i]);
                db2[i] += delta2[i];
                for (size_t j = 0; j < hidden_dim_; ++j) {
                    dW2[i][j] += delta2[i] * a1[j];
                }
            }

            // 3. Hidden error: delta1 = (W2^T * delta2) \odot (1 - a1^2)
            for (size_t j = 0; j < hidden_dim_; ++j) {
                double dL_da1 = 0.0;
                for (size_t i = 0; i < out_dim_; ++i) {
                    dL_da1 += delta2[i] * W2_[i][j];
                }
                double delta1_j = dL_da1 * (1.0 - a1[j] * a1[j]);
                db1[j] += delta1_j;
                for (size_t m = 0; m < in_dim_; ++m) {
                    dW1[j][m] += delta1_j * x[m];
                }
            }
        }

        // Flatten gradient matrices into contiguous 1D vector
        std::vector<double> flat_grads;
        flat_grads.reserve(total_parameters());

        for (const auto& row : dW1) {
            for (double g : row) flat_grads.push_back(g);
        }
        for (double g : db1) flat_grads.push_back(g);

        for (const auto& row : dW2) {
            for (double g : row) flat_grads.push_back(g);
        }
        for (double g : db2) flat_grads.push_back(g);

        return flat_grads;
    }

    /**
     * @brief Numerical gradient calculation via central finite differences:
     *        g_j \approx [L(\theta + \epsilon * e_j) - L(\theta - \epsilon * e_j)] / (2 * \epsilon)
     */
    std::vector<double> finite_difference_gradients(
        const std::vector<std::vector<double>>& X,
        const std::vector<std::vector<double>>& Y,
        double epsilon = 1e-6) {
        auto original_params = get_parameters();
        const size_t num_params = original_params.size();
        std::vector<double> num_grads(num_params, 0.0);

        auto perturbed_params = original_params;

        for (size_t j = 0; j < num_params; ++j) {
            perturbed_params[j] = original_params[j] + epsilon;
            set_parameters(perturbed_params);
            double loss_plus = compute_loss(X, Y);

            perturbed_params[j] = original_params[j] - epsilon;
            set_parameters(perturbed_params);
            double loss_minus = compute_loss(X, Y);

            num_grads[j] = (loss_plus - loss_minus) / (2.0 * epsilon);
            perturbed_params[j] = original_params[j]; // Reset
        }

        // Restore original state
        set_parameters(original_params);
        return num_grads;
    }

private:
    size_t in_dim_;
    size_t hidden_dim_;
    size_t out_dim_;

    std::vector<std::vector<double>> W1_;
    std::vector<double> b1_;
    std::vector<std::vector<double>> W2_;
    std::vector<double> b2_;

    void initialize_parameters(unsigned int seed) {
        std::mt19937 gen(seed);

        // Xavier / Glorot uniform initialization: [-limit, limit]
        // limit = sqrt(6 / (fan_in + fan_out))
        double limit1 = std::sqrt(6.0 / static_cast<double>(in_dim_ + hidden_dim_));
        std::uniform_real_distribution<double> dist1(-limit1, limit1);

        W1_.assign(hidden_dim_, std::vector<double>(in_dim_));
        for (size_t i = 0; i < hidden_dim_; ++i) {
            for (size_t j = 0; j < in_dim_; ++j) {
                W1_[i][j] = dist1(gen);
            }
        }
        b1_.assign(hidden_dim_, 0.0);

        double limit2 = std::sqrt(6.0 / static_cast<double>(hidden_dim_ + out_dim_));
        std::uniform_real_distribution<double> dist2(-limit2, limit2);

        W2_.assign(out_dim_, std::vector<double>(hidden_dim_));
        for (size_t i = 0; i < out_dim_; ++i) {
            for (size_t j = 0; j < hidden_dim_; ++j) {
                W2_[i][j] = dist2(gen);
            }
        }
        b2_.assign(out_dim_, 0.0);
    }

    void validate_dataset(
        const std::vector<std::vector<double>>& X,
        const std::vector<std::vector<double>>& Y) const {
        if (X.empty()) {
            throw std::invalid_argument("TwoLayerMLP: input dataset X cannot be empty.");
        }
        if (X.size() != Y.size()) {
            throw std::invalid_argument("TwoLayerMLP: sample count mismatch between X and Y.");
        }
        for (size_t i = 0; i < X.size(); ++i) {
            if (X[i].size() != in_dim_) {
                throw std::invalid_argument("TwoLayerMLP: X sample dimension mismatch at index " + std::to_string(i));
            }
            if (Y[i].size() != out_dim_) {
                throw std::invalid_argument("TwoLayerMLP: Y sample dimension mismatch at index " + std::to_string(i));
            }
        }
    }
};

} // namespace opt

#endif // NEURAL_NET_HPP
