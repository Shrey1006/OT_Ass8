/**
 * @file objectives.hpp
 * @brief Continuous benchmark objective functions and analytical gradients.
 *
 * Implements:
 *  1. ConvexQuadratic: Standard isotropic/well-conditioned convex quadratic.
 *  2. StronglyConvexQuadratic: Ill-conditioned quadratic with condition number kappa = 100.
 *  3. Rosenbrock: Classic non-convex banana valley benchmark.
 *  4. SaddlePoint: f(x,y) = x^2 - y^2 (indefinite Hessian, saddle point at origin).
 *  5. Rastrigin: Highly multimodal landscape with numerous local minima and 1 global minimum.
 *  6. Himmelblau: Multimodal benchmark with 4 distinct local/global minima and saddle points.
 *
 * Provides central finite differences utility for gradient verification.
 * Fixed random seeds and strict input validation enforced throughout.
 *
 * Optimization Techniques | Lab Assignment 8 (Track A)
 */

#ifndef OBJECTIVES_HPP
#define OBJECTIVES_HPP

#include <vector>
#include <cmath>
#include <stdexcept>
#include <string>
#include <sstream>
#include <cstddef>

namespace opt {

/**
 * @brief Abstract base class for optimization objective functions.
 */
class ObjectiveFunction {
public:
    virtual ~ObjectiveFunction() = default;

    /**
     * @brief Computes scalar objective function value f(x).
     */
    virtual double value(const std::vector<double>& x) const = 0;

    /**
     * @brief Computes analytical gradient vector \nabla f(x).
     */
    virtual std::vector<double> gradient(const std::vector<double>& x) const = 0;

    /**
     * @brief Expected dimension of the parameter vector.
     */
    virtual size_t dimension() const noexcept = 0;

    /**
     * @brief Human-readable name of the objective function.
     */
    virtual const char* name() const noexcept = 0;

    /**
     * @brief Computes numerical gradient using central finite differences:
     *        df/dx_i \approx [f(x + h*e_i) - f(x - h*e_i)] / (2*h)
     *        Truncation error is O(h^2).
     * @param x Point at which to evaluate gradient.
     * @param h Perturbation step size (default 1e-6).
     */
    std::vector<double> finite_difference_gradient(const std::vector<double>& x, double h = 1e-6) const {
        validate_input(x);
        if (h <= 0.0 || !std::isfinite(h)) {
            throw std::invalid_argument("finite_difference_gradient: step size h must be positive and finite.");
        }

        const size_t dim = dimension();
        std::vector<double> num_grad(dim, 0.0);
        std::vector<double> x_forward = x;
        std::vector<double> x_backward = x;

        for (size_t i = 0; i < dim; ++i) {
            x_forward[i] = x[i] + h;
            x_backward[i] = x[i] - h;

            const double f_plus = value(x_forward);
            const double f_minus = value(x_backward);

            num_grad[i] = (f_plus - f_minus) / (2.0 * h);

            // Restore coordinate
            x_forward[i] = x[i];
            x_backward[i] = x[i];
        }

        return num_grad;
    }

protected:
    void validate_input(const std::vector<double>& x) const {
        if (x.size() != dimension()) {
            std::ostringstream oss;
            oss << name() << ": input dimension mismatch. Expected "
                << dimension() << ", got " << x.size();
            throw std::invalid_argument(oss.str());
        }
        for (size_t i = 0; i < x.size(); ++i) {
            if (!std::isfinite(x[i])) {
                throw std::invalid_argument(std::string(name()) + ": non-finite input at index " + std::to_string(i));
            }
        }
    }
};

/**
 * @brief Standard 2D Convex Quadratic Function:
 *        f(x, y) = 0.5 * (x^2 + 2*y^2)
 *
 * Eigenvalues of Hessian: lambda_1 = 1, lambda_2 = 2.
 * Condition number: kappa = 2.0 (well-conditioned).
 * Minimum: f(0, 0) = 0.
 */
class ConvexQuadratic : public ObjectiveFunction {
public:
    double value(const std::vector<double>& x) const override {
        validate_input(x);
        return 0.5 * (x[0] * x[0] + 2.0 * x[1] * x[1]);
    }

    std::vector<double> gradient(const std::vector<double>& x) const override {
        validate_input(x);
        return {x[0], 2.0 * x[1]};
    }

    size_t dimension() const noexcept override { return 2; }
    const char* name() const noexcept override { return "ConvexQuadratic"; }
};

/**
 * @brief Strongly Convex Quadratic with Condition Number kappa = 100:
 *        f(x, y) = 0.5 * (x^2 + 100 * y^2)
 *
 * Hessian matrix: H = diag(1, 100).
 * Eigenvalues: lambda_min = 1.0, lambda_max = 100.0.
 * Condition number: kappa = lambda_max / lambda_min = 100.0.
 * Strong convexity parameter mu = 1.0, Lipschitz gradient constant L = 100.0.
 * Minimum: f(0, 0) = 0.
 */
class StronglyConvexQuadratic : public ObjectiveFunction {
public:
    double value(const std::vector<double>& x) const override {
        validate_input(x);
        return 0.5 * (x[0] * x[0] + 100.0 * x[1] * x[1]);
    }

    std::vector<double> gradient(const std::vector<double>& x) const override {
        validate_input(x);
        return {x[0], 100.0 * x[1]};
    }

    size_t dimension() const noexcept override { return 2; }
    const char* name() const noexcept override { return "StronglyConvexQuadratic (kappa=100)"; }
    double condition_number() const noexcept { return 100.0; }
};

/**
 * @brief Rosenbrock "Banana" Function:
 *        f(x, y) = (a - x)^2 + b * (y - x^2)^2
 * Standard parameters: a = 1, b = 100.
 *
 * Gradients:
 *   df/dx = 2*(x - a) - 4*b*x*(y - x^2)
 *   df/dy = 2*b*(y - x^2)
 *
 * Global minimum: f(1, 1) = 0.
 * Features a steep, narrow curved parabolic valley.
 */
class Rosenbrock : public ObjectiveFunction {
public:
    explicit Rosenbrock(double a = 1.0, double b = 100.0)
        : a_(a), b_(b) {
        if (!std::isfinite(a_) || !std::isfinite(b_) || b_ <= 0.0) {
            throw std::invalid_argument("Rosenbrock: parameters must be finite and b > 0.");
        }
    }

    double value(const std::vector<double>& x) const override {
        validate_input(x);
        const double term1 = a_ - x[0];
        const double term2 = x[1] - x[0] * x[0];
        return term1 * term1 + b_ * term2 * term2;
    }

    std::vector<double> gradient(const std::vector<double>& x) const override {
        validate_input(x);
        const double df_dx = 2.0 * (x[0] - a_) - 4.0 * b_ * x[0] * (x[1] - x[0] * x[0]);
        const double df_dy = 2.0 * b_ * (x[1] - x[0] * x[0]);
        return {df_dx, df_dy};
    }

    size_t dimension() const noexcept override { return 2; }
    const char* name() const noexcept override { return "Rosenbrock"; }

private:
    double a_;
    double b_;
};

/**
 * @brief Saddle Point Objective Function:
 *        f(x, y) = x^2 - y^2
 *
 * Hessian matrix: H = diag(2, -2).
 * Eigenvalues: +2 (convex along x), -2 (concave along y).
 * Critical point at (0, 0) is a strict saddle point (\nabla f(0,0) = 0, det(H) < 0).
 *
 * Gradients:
 *   df/dx = 2*x
 *   df/dy = -2*y
 */
class SaddlePoint : public ObjectiveFunction {
public:
    double value(const std::vector<double>& x) const override {
        validate_input(x);
        return x[0] * x[0] - x[1] * x[1];
    }

    std::vector<double> gradient(const std::vector<double>& x) const override {
        validate_input(x);
        return {2.0 * x[0], -2.0 * x[1]};
    }

    size_t dimension() const noexcept override { return 2; }
    const char* name() const noexcept override { return "SaddlePoint (x^2 - y^2)"; }
};

/**
 * @brief 2D Rastrigin Function (Multiple Local Minima):
 *        f(x, y) = 20 + [x^2 - 10*cos(2*pi*x)] + [y^2 - 10*cos(2*pi*y)]
 *
 * Highly non-convex surface with a regular grid of local minima.
 * Global minimum: f(0, 0) = 0.
 *
 * Gradients:
 *   df/dx = 2*x + 20*pi*sin(2*pi*x)
 *   df/dy = 2*y + 20*pi*sin(2*pi*y)
 */
class Rastrigin : public ObjectiveFunction {
public:
    static constexpr double PI = 3.14159265358979323846;

    double value(const std::vector<double>& x) const override {
        validate_input(x);
        return 20.0 + (x[0] * x[0] - 10.0 * std::cos(2.0 * PI * x[0]))
                    + (x[1] * x[1] - 10.0 * std::cos(2.0 * PI * x[1]));
    }

    std::vector<double> gradient(const std::vector<double>& x) const override {
        validate_input(x);
        const double df_dx = 2.0 * x[0] + 20.0 * PI * std::sin(2.0 * PI * x[0]);
        const double df_dy = 2.0 * x[1] + 20.0 * PI * std::sin(2.0 * PI * x[1]);
        return {df_dx, df_dy};
    }

    size_t dimension() const noexcept override { return 2; }
    const char* name() const noexcept override { return "Rastrigin"; }
};

/**
 * @brief Himmelblau's Function (Multimodal Benchmark):
 *        f(x, y) = (x^2 + y - 11)^2 + (x + y^2 - 7)^2
 *
 * Features 4 identical local/global minima where f(x, y) = 0:
 *   1. (3.0, 2.0)
 *   2. (-2.805118, 3.131312)
 *   3. (-3.779310, -3.283186)
 *   4. (3.584428, -1.848126)
 *
 * Gradients:
 *   df/dx = 4*x*(x^2 + y - 11) + 2*(x + y^2 - 7)
 *   df/dy = 2*(x^2 + y - 11) + 4*y*(x + y^2 - 7)
 */
class Himmelblau : public ObjectiveFunction {
public:
    double value(const std::vector<double>& x) const override {
        validate_input(x);
        const double term1 = x[0] * x[0] + x[1] - 11.0;
        const double term2 = x[0] + x[1] * x[1] - 7.0;
        return term1 * term1 + term2 * term2;
    }

    std::vector<double> gradient(const std::vector<double>& x) const override {
        validate_input(x);
        const double term1 = x[0] * x[0] + x[1] - 11.0;
        const double term2 = x[0] + x[1] * x[1] - 7.0;
        const double df_dx = 4.0 * x[0] * term1 + 2.0 * term2;
        const double df_dy = 2.0 * term1 + 4.0 * x[1] * term2;
        return {df_dx, df_dy};
    }

    size_t dimension() const noexcept override { return 2; }
    const char* name() const noexcept override { return "Himmelblau"; }
};

} // namespace opt

#endif // OBJECTIVES_HPP
