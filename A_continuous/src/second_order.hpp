/**
 * @file second_order.hpp
 * @brief Second-order and curvature-based continuous optimization methods in C++17.
 *
 * Implements:
 *  1. Newton's Method with Levenberg-Marquardt regularization and backtracking line search.
 *  2. Gauss-Newton algorithm for nonlinear least-squares curve fitting.
 *  3. Hessian-Vector Products (HVP) via central finite differences O(\epsilon^2).
 *  4. Empirical Diagonal Fisher Information Matrix (FIM) approximation.
 *  5. Low-Rank Curvature Approximation (L-BFGS two-loop inverse Hessian recursion).
 *
 * All implementations feature strict input validation, deterministic execution,
 * and comprehensive documentation of optimization logic.
 *
 * Optimization Techniques | Lab Assignment 8 (Track A)
 */

#ifndef SECOND_ORDER_HPP
#define SECOND_ORDER_HPP

#include <vector>
#include <cmath>
#include <stdexcept>
#include <string>
#include <sstream>
#include <cstddef>
#include <algorithm>
#include <deque>

namespace opt {

/**
 * @brief Solves a linear system A * x = b via Gaussian elimination with partial pivoting.
 * @param A Square matrix of size n x n.
 * @param b Right-hand side vector of size n.
 * @return Solution vector x of size n.
 * @throws std::invalid_argument If dimensions mismatch or if matrix is singular.
 */
inline std::vector<double> solve_linear_system(
    std::vector<std::vector<double>> A,
    std::vector<double> b) {
    const size_t n = b.size();
    if (A.size() != n) {
        throw std::invalid_argument("solve_linear_system: matrix row count does not match vector size.");
    }
    for (size_t i = 0; i < n; ++i) {
        if (A[i].size() != n) {
            throw std::invalid_argument("solve_linear_system: matrix is not square.");
        }
    }

    // Forward elimination with partial pivoting
    for (size_t col = 0; col < n; ++col) {
        size_t max_row = col;
        double max_val = std::abs(A[col][col]);
        for (size_t r = col + 1; r < n; ++r) {
            double v = std::abs(A[r][col]);
            if (v > max_val) {
                max_val = v;
                max_row = r;
            }
        }

        if (max_val < 1e-14) {
            throw std::runtime_error("solve_linear_system: singular or near-singular matrix encountered.");
        }

        if (max_row != col) {
            std::swap(A[col], A[max_row]);
            std::swap(b[col], b[max_row]);
        }

        for (size_t r = col + 1; r < n; ++r) {
            double factor = A[r][col] / A[col][col];
            for (size_t c = col; c < n; ++c) {
                A[r][c] -= factor * A[col][c];
            }
            b[r] -= factor * b[col];
        }
    }

    // Back substitution
    std::vector<double> x(n, 0.0);
    for (int r = static_cast<int>(n) - 1; r >= 0; --r) {
        double sum = b[r];
        for (size_t c = static_cast<size_t>(r) + 1; c < n; ++c) {
            sum -= A[r][c] * x[c];
        }
        x[r] = sum / A[r][r];
    }

    return x;
}

/**
 * @brief Computes Hessian-Vector Product (HVP) \nabla^2 f(x) * v via central finite differences:
 *        HVP(v) = [\nabla f(x + \epsilon * v) - \nabla f(x - \epsilon * v)] / (2 * \epsilon)
 *
 * Avoids constructing or storing the full d x d Hessian matrix, with truncation error O(\epsilon^2).
 */
template <typename GradFunc>
std::vector<double> hessian_vector_product(
    GradFunc&& grad_fn,
    const std::vector<double>& x,
    const std::vector<double>& v,
    double epsilon = 1e-5) {
    if (x.size() != v.size()) {
        throw std::invalid_argument("hessian_vector_product: dimension mismatch between x and v.");
    }
    if (epsilon <= 0.0 || !std::isfinite(epsilon)) {
        throw std::invalid_argument("hessian_vector_product: epsilon must be strictly positive and finite.");
    }

    const size_t dim = x.size();
    std::vector<double> x_plus(dim), x_minus(dim);
    for (size_t i = 0; i < dim; ++i) {
        x_plus[i] = x[i] + epsilon * v[i];
        x_minus[i] = x[i] - epsilon * v[i];
    }

    auto g_plus = grad_fn(x_plus);
    auto g_minus = grad_fn(x_minus);

    if (g_plus.size() != dim || g_minus.size() != dim) {
        throw std::runtime_error("hessian_vector_product: gradient dimension does not match parameter dimension.");
    }

    std::vector<double> hvp(dim, 0.0);
    for (size_t i = 0; i < dim; ++i) {
        hvp[i] = (g_plus[i] - g_minus[i]) / (2.0 * epsilon);
    }
    return hvp;
}

/**
 * @brief Newton's Optimization Method.
 *
 * Solves:
 *   [H(x_t) + \lambda * I] * \Delta x = -\nabla f(x_t)
 *   x_{t+1} = x_t + \alpha * \Delta x
 *
 * Features:
 *  - Damping parameter \lambda >= 0 to ensure positive definiteness.
 *  - Backtracking Armijo line search to ensure monotonic descent.
 */
class NewtonOptimizer {
public:
    struct Options {
        size_t max_iterations;
        double tolerance;
        double lambda_reg;
        double armijo_c1;
        double line_search_decay;

        Options(size_t max_it = 100, double tol = 1e-8, double l_reg = 1e-4,
                double armijo = 1e-4, double decay = 0.5)
            : max_iterations(max_it), tolerance(tol), lambda_reg(l_reg),
              armijo_c1(armijo), line_search_decay(decay) {}
    };

    struct Result {
        std::vector<double> point;
        double final_loss = 0.0;
        size_t iterations = 0;
        bool converged = false;
    };

    NewtonOptimizer() : NewtonOptimizer(Options{}) {}

    explicit NewtonOptimizer(const Options& opts) : opts_(opts) {
        if (opts_.tolerance <= 0.0) throw std::invalid_argument("NewtonOptimizer: tolerance must be > 0.");
        if (opts_.lambda_reg < 0.0) throw std::invalid_argument("NewtonOptimizer: lambda_reg must be >= 0.");
        if (opts_.armijo_c1 <= 0.0 || opts_.armijo_c1 >= 1.0)
            throw std::invalid_argument("NewtonOptimizer: armijo_c1 must be in (0, 1).");
    }

    template <typename ValFunc, typename GradFunc, typename HessFunc>
    Result minimize(
        ValFunc&& val_fn,
        GradFunc&& grad_fn,
        HessFunc&& hess_fn,
        std::vector<double> x0) const {
        const size_t dim = x0.size();
        if (dim == 0) {
            throw std::invalid_argument("NewtonOptimizer: empty initial parameter vector.");
        }

        Result res;
        res.point = std::move(x0);

        for (size_t iter = 0; iter < opts_.max_iterations; ++iter) {
            res.iterations = iter + 1;
            const double current_val = val_fn(res.point);
            const auto grad = grad_fn(res.point);
            auto hess = hess_fn(res.point);

            double grad_norm = 0.0;
            for (double g : grad) grad_norm += g * g;
            grad_norm = std::sqrt(grad_norm);

            if (grad_norm < opts_.tolerance) {
                res.converged = true;
                res.final_loss = current_val;
                return res;
            }

            for (size_t i = 0; i < dim; ++i) {
                hess[i][i] += opts_.lambda_reg;
            }

            std::vector<double> neg_grad(dim);
            for (size_t i = 0; i < dim; ++i) neg_grad[i] = -grad[i];

            std::vector<double> delta = solve_linear_system(hess, neg_grad);

            double alpha = 1.0;
            double directional_derivative = 0.0;
            for (size_t i = 0; i < dim; ++i) {
                directional_derivative += grad[i] * delta[i];
            }

            std::vector<double> candidate(dim);
            while (alpha > 1e-8) {
                for (size_t i = 0; i < dim; ++i) {
                    candidate[i] = res.point[i] + alpha * delta[i];
                }
                double new_val = val_fn(candidate);
                if (new_val <= current_val + opts_.armijo_c1 * alpha * directional_derivative) {
                    res.point = candidate;
                    break;
                }
                alpha *= opts_.line_search_decay;
            }

            if (alpha <= 1e-8) {
                res.final_loss = val_fn(res.point);
                return res;
            }
        }

        res.final_loss = val_fn(res.point);
        return res;
    }

private:
    Options opts_;
};

/**
 * @brief Gauss-Newton Algorithm for Nonlinear Least Squares Curve Fitting:
 *        min_\theta 0.5 * \sum_{i=1}^N (y_i - f(x_i; \theta))^2
 *
 * Normal equations:
 *   [J^T * J + \lambda * I] * \Delta \theta = J^T * r
 *   where r_i = y_i - f(x_i; \theta) is the residual and J_ij = \partial f(x_i) / \partial \theta_j.
 */
class GaussNewtonCurveFit {
public:
    struct Options {
        size_t max_iterations;
        double tolerance;
        double lambda_reg;

        Options(size_t max_it = 100, double tol = 1e-7, double l_reg = 1e-6)
            : max_iterations(max_it), tolerance(tol), lambda_reg(l_reg) {}
    };

    struct Result {
        std::vector<double> params;
        double sse = 0.0;
        size_t iterations = 0;
        bool converged = false;
    };

    GaussNewtonCurveFit() : GaussNewtonCurveFit(Options{}) {}

    explicit GaussNewtonCurveFit(const Options& opts) : opts_(opts) {}

    template <typename ModelFunc, typename JacFunc>
    Result fit(
        const std::vector<double>& x_data,
        const std::vector<double>& y_data,
        ModelFunc&& model_fn,
        JacFunc&& jacobian_fn,
        std::vector<double> theta0) const {
        const size_t N = x_data.size();
        if (N != y_data.size()) {
            throw std::invalid_argument("GaussNewtonCurveFit: x_data and y_data size mismatch.");
        }
        const size_t p = theta0.size();
        if (p == 0 || N < p) {
            throw std::invalid_argument("GaussNewtonCurveFit: need at least as many data points as parameters.");
        }

        Result res;
        res.params = std::move(theta0);

        for (size_t iter = 0; iter < opts_.max_iterations; ++iter) {
            res.iterations = iter + 1;

            std::vector<double> r(N);
            std::vector<std::vector<double>> J(N, std::vector<double>(p));
            double sse = 0.0;

            for (size_t i = 0; i < N; ++i) {
                double pred = model_fn(x_data[i], res.params);
                r[i] = y_data[i] - pred;
                sse += r[i] * r[i];
                J[i] = jacobian_fn(x_data[i], res.params);
            }
            res.sse = 0.5 * sse;

            std::vector<std::vector<double>> JtJ(p, std::vector<double>(p, 0.0));
            std::vector<double> JtR(p, 0.0);

            for (size_t j = 0; j < p; ++j) {
                for (size_t k = 0; k < p; ++k) {
                    double sum = 0.0;
                    for (size_t i = 0; i < N; ++i) {
                        sum += J[i][j] * J[i][k];
                    }
                    JtJ[j][k] = sum;
                }
                double r_sum = 0.0;
                for (size_t i = 0; i < N; ++i) {
                    r_sum += J[i][j] * r[i];
                }
                JtR[j] = r_sum;
            }

            for (size_t j = 0; j < p; ++j) {
                JtJ[j][j] += opts_.lambda_reg;
            }

            std::vector<double> delta = solve_linear_system(JtJ, JtR);

            double delta_norm = 0.0;
            for (size_t j = 0; j < p; ++j) {
                res.params[j] += delta[j];
                delta_norm += delta[j] * delta[j];
            }
            delta_norm = std::sqrt(delta_norm);

            if (delta_norm < opts_.tolerance) {
                res.converged = true;
                double final_sse = 0.0;
                for (size_t i = 0; i < N; ++i) {
                    double residual = y_data[i] - model_fn(x_data[i], res.params);
                    final_sse += residual * residual;
                }
                res.sse = 0.5 * final_sse;
                return res;
            }
        }

        return res;
    }

private:
    Options opts_;
};

/**
 * @brief Empirical Diagonal Fisher Information Matrix (FIM) Approximation.
 *
 * For a batch of N sample gradients g_i = \nabla \ell_i(\theta):
 *   F_diag = (1 / N) * \sum_{i=1}^N g_i \odot g_i
 */
inline std::vector<double> compute_diagonal_fisher(
    const std::vector<std::vector<double>>& per_sample_gradients) {
    if (per_sample_gradients.empty()) {
        throw std::invalid_argument("compute_diagonal_fisher: sample gradient list is empty.");
    }
    const size_t N = per_sample_gradients.size();
    const size_t dim = per_sample_gradients[0].size();
    if (dim == 0) {
        throw std::invalid_argument("compute_diagonal_fisher: parameter dimension is zero.");
    }

    std::vector<double> diag_fisher(dim, 0.0);
    for (size_t i = 0; i < N; ++i) {
        if (per_sample_gradients[i].size() != dim) {
            throw std::invalid_argument("compute_diagonal_fisher: dimension mismatch across samples.");
        }
        for (size_t j = 0; j < dim; ++j) {
            diag_fisher[j] += per_sample_gradients[i][j] * per_sample_gradients[i][j];
        }
    }

    const double inv_N = 1.0 / static_cast<double>(N);
    for (size_t j = 0; j < dim; ++j) {
        diag_fisher[j] *= inv_N;
    }

    return diag_fisher;
}

/**
 * @brief Low-Rank Curvature Approximation via Limited-Memory BFGS (L-BFGS).
 *
 * Stores the m most recent displacement pairs:
 *   s_k = x_{k+1} - x_k
 *   y_k = g_{k+1} - g_k
 *
 * Applies the classic two-loop recursion to compute the matrix-vector product
 * H_k * v \approx \nabla^2 f(x)^{-1} * v in O(m * d) time.
 */
class LowRankCurvatureLBFGS {
public:
    explicit LowRankCurvatureLBFGS(size_t history_size = 5)
        : m_(history_size) {
        if (m_ == 0) throw std::invalid_argument("LowRankCurvatureLBFGS: history_size must be >= 1.");
    }

    void update(const std::vector<double>& s, const std::vector<double>& y) {
        if (s.size() != y.size()) {
            throw std::invalid_argument("LowRankCurvatureLBFGS: s and y dimension mismatch.");
        }
        double sy = 0.0;
        for (size_t i = 0; i < s.size(); ++i) {
            sy += s[i] * y[i];
        }

        if (sy <= 1e-10) {
            return;
        }

        if (s_history_.size() >= m_) {
            s_history_.pop_front();
            y_history_.pop_front();
            rho_history_.pop_front();
        }

        s_history_.push_back(s);
        y_history_.push_back(y);
        rho_history_.push_back(1.0 / sy);
    }

    std::vector<double> multiply_inverse_hessian(const std::vector<double>& v) const {
        const size_t k = s_history_.size();
        if (k == 0) {
            return v;
        }

        const size_t dim = v.size();
        std::vector<double> q = v;
        std::vector<double> alpha(k);

        for (int i = static_cast<int>(k) - 1; i >= 0; --i) {
            double sq = 0.0;
            for (size_t d = 0; d < dim; ++d) {
                sq += s_history_[i][d] * q[d];
            }
            alpha[i] = rho_history_[i] * sq;
            for (size_t d = 0; d < dim; ++d) {
                q[d] -= alpha[i] * y_history_[i][d];
            }
        }

        double y_dot_y = 0.0;
        for (size_t d = 0; d < dim; ++d) {
            y_dot_y += y_history_.back()[d] * y_history_.back()[d];
        }
        double gamma = (y_dot_y > 1e-12) ? (1.0 / (rho_history_.back() * y_dot_y)) : 1.0;

        std::vector<double> r(dim);
        for (size_t d = 0; d < dim; ++d) {
            r[d] = gamma * q[d];
        }

        for (size_t i = 0; i < k; ++i) {
            double yr = 0.0;
            for (size_t d = 0; d < dim; ++d) {
                yr += y_history_[i][d] * r[d];
            }
            double beta = rho_history_[i] * yr;
            for (size_t d = 0; d < dim; ++d) {
                r[d] += s_history_[i][d] * (alpha[i] - beta);
            }
        }

        return r;
    }

    void reset() noexcept {
        s_history_.clear();
        y_history_.clear();
        rho_history_.clear();
    }

    size_t memory_count() const noexcept { return s_history_.size(); }

private:
    size_t m_;
    std::deque<std::vector<double>> s_history_;
    std::deque<std::vector<double>> y_history_;
    std::deque<double> rho_history_;
};

} // namespace opt

#endif // SECOND_ORDER_HPP
