/**
 * @file test_main.cpp
 * @brief Comprehensive unit tests for continuous optimizers, benchmark objectives,
 *        and second-order curvature methods.
 *
 * Verifies:
 *  1. Input validation (dimensions, bounds, non-finite values).
 *  2. Analytical correctness of optimizer single-step updates against manual math.
 *  3. Reset behavior restoring clean initial state.
 *  4. Convergence on a 2D convex quadratic surface.
 *  5. Objective function analytical gradients verified against central finite differences O(h^2):
 *     - ConvexQuadratic
 *     - StronglyConvexQuadratic (condition number kappa = 100)
 *     - Rosenbrock
 *     - SaddlePoint (f(x, y) = x^2 - y^2)
 *     - Rastrigin (highly multimodal)
 *     - Himmelblau (4 distinct minima)
 *  6. Second-order methods against known solutions:
 *     - Pure Newton method exact 1-step solve on quadratic and convergence on Rosenbrock.
 *     - Gauss-Newton nonlinear curve fitting recovering ground truth parameters.
 *     - Finite-difference Hessian-Vector Product (HVP) matching exact matrix product.
 *     - Diagonal Fisher Information Matrix matching analytical expectations.
 *     - Low-rank L-BFGS curvature approximation satisfying the inverse secant equation.
 *
 * Optimization Techniques | Lab Assignment 8 (Track A)
 */

#include "optimizers.hpp"
#include "objectives.hpp"
#include "second_order.hpp"
#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>
#include <iomanip>
#include <memory>
#include <limits>

namespace {

bool approx_equal(double a, double b, double tol = 1e-5) {
    return std::abs(a - b) <= tol;
}

void test_input_validation() {
    std::cout << "[TEST] Running input validation tests for optimizers..." << std::endl;

    // 1. Invalid learning rates
    try {
        opt::GradientDescent gd(-0.01);
        assert(false && "Failed to throw on negative learning rate");
    } catch (const std::invalid_argument&) {}

    try {
        opt::SGD sgd(0.0);
        assert(false && "Failed to throw on zero learning rate");
    } catch (const std::invalid_argument&) {}

    // 2. Invalid batch size
    try {
        opt::MiniBatchSGD mb(0.01, 0);
        assert(false && "Failed to throw on zero batch size");
    } catch (const std::invalid_argument&) {}

    // 3. Invalid momentum beta
    try {
        opt::Momentum mom(0.01, 1.0);
        assert(false && "Failed to throw on beta >= 1.0");
    } catch (const std::invalid_argument&) {}

    try {
        opt::NAG nag(0.01, -0.1);
        assert(false && "Failed to throw on negative beta");
    } catch (const std::invalid_argument&) {}

    // 4. Invalid epsilon
    try {
        opt::AdaGrad ada(0.01, 0.0);
        assert(false && "Failed to throw on non-positive epsilon");
    } catch (const std::invalid_argument&) {}

    try {
        opt::Adam adam(0.01, 0.9, 0.999, -1e-5);
        assert(false && "Failed to throw on negative epsilon");
    } catch (const std::invalid_argument&) {}

    // 5. Dimension mismatch
    opt::GradientDescent gd(0.1);
    std::vector<double> params = {1.0, 2.0};
    std::vector<double> grads_mismatch = {0.1};
    try {
        gd.step(params, grads_mismatch);
        assert(false && "Failed to throw on dimension mismatch");
    } catch (const std::invalid_argument&) {}

    // 6. Non-finite values
    std::vector<double> nan_grads = {0.1, std::numeric_limits<double>::quiet_NaN()};
    try {
        gd.step(params, nan_grads);
        assert(false && "Failed to throw on NaN in gradients");
    } catch (const std::invalid_argument&) {}

    std::vector<double> inf_params = {std::numeric_limits<double>::infinity(), 2.0};
    std::vector<double> valid_grads = {0.1, 0.2};
    try {
        gd.step(inf_params, valid_grads);
        assert(false && "Failed to throw on Inf in parameters");
    } catch (const std::invalid_argument&) {}

    std::cout << "  -> Optimizer input validation passed successfully." << std::endl;
}

void test_analytical_single_step() {
    std::cout << "[TEST] Running analytical single-step numerical verification..." << std::endl;

    const std::vector<double> initial_params = {1.0, 2.0};
    const std::vector<double> grads = {0.5, -0.25};

    // 1. GradientDescent
    {
        opt::GradientDescent gd(0.1);
        auto p = initial_params;
        gd.step(p, grads);
        assert(approx_equal(p[0], 0.95));
        assert(approx_equal(p[1], 2.025));
    }

    // 2. SGD
    {
        opt::SGD sgd(0.1);
        auto p = initial_params;
        sgd.step(p, grads);
        assert(approx_equal(p[0], 0.95));
        assert(approx_equal(p[1], 2.025));
    }

    // 3. MiniBatchSGD
    {
        opt::MiniBatchSGD mb(0.1, 4);
        auto p = initial_params;
        mb.step(p, grads);
        assert(approx_equal(p[0], 0.95));
        assert(approx_equal(p[1], 2.025));
    }

    // 4. Momentum
    {
        opt::Momentum mom(0.1, 0.9);
        auto p = initial_params;
        mom.step(p, grads);
        assert(approx_equal(p[0], 0.95));
        assert(approx_equal(p[1], 2.025));

        std::vector<double> g2 = {0.1, 0.2};
        mom.step(p, g2);
        assert(approx_equal(p[0], 0.895));
    }

    // 5. NAG
    {
        opt::NAG nag(0.1, 0.9);
        auto p = initial_params;
        nag.step(p, grads);
        assert(approx_equal(p[0], 0.905));
        assert(approx_equal(p[1], 2.0475));
    }

    // 6. AdaGrad
    {
        opt::AdaGrad ada(0.1, 1e-8);
        auto p = initial_params;
        ada.step(p, grads);
        assert(approx_equal(p[0], 0.90));
        assert(approx_equal(p[1], 2.10));
    }

    // 7. RMSProp
    {
        opt::RMSProp rmsprop(0.1, 0.9, 1e-8);
        auto p = initial_params;
        rmsprop.step(p, grads);
        double expected_v0 = 0.1 * 0.25;
        double expected_p0 = 1.0 - (0.1 / (std::sqrt(expected_v0) + 1e-8)) * 0.5;
        assert(approx_equal(p[0], expected_p0));
    }

    // 8. Adam
    {
        opt::Adam adam(0.1, 0.9, 0.999, 1e-8);
        auto p = initial_params;
        adam.step(p, grads);
        assert(approx_equal(p[0], 0.90));
    }

    std::cout << "  -> Analytical single-step checks passed." << std::endl;
}

void test_reset_behavior() {
    std::cout << "[TEST] Running reset() state consistency tests..." << std::endl;

    std::vector<std::unique_ptr<opt::Optimizer>> optimizers;
    optimizers.push_back(std::make_unique<opt::GradientDescent>(0.05));
    optimizers.push_back(std::make_unique<opt::SGD>(0.05));
    optimizers.push_back(std::make_unique<opt::MiniBatchSGD>(0.05, 8));
    optimizers.push_back(std::make_unique<opt::Momentum>(0.05, 0.9));
    optimizers.push_back(std::make_unique<opt::NAG>(0.05, 0.9));
    optimizers.push_back(std::make_unique<opt::AdaGrad>(0.05));
    optimizers.push_back(std::make_unique<opt::RMSProp>(0.05, 0.9));
    optimizers.push_back(std::make_unique<opt::Adam>(0.05));

    const std::vector<double> p0 = {3.0, -4.0};
    const std::vector<double> g0 = {1.5, -2.0};
    const std::vector<double> g1 = {0.8, -1.1};

    for (auto& opt : optimizers) {
        auto p_run1 = p0;
        opt->step(p_run1, g0);
        auto first_step_result = p_run1;

        opt->step(p_run1, g1);

        opt->reset();
        assert(opt->get_step_count() == 0);

        auto p_run2 = p0;
        opt->step(p_run2, g0);
        assert(approx_equal(p_run2[0], first_step_result[0], 1e-12));
        assert(approx_equal(p_run2[1], first_step_result[1], 1e-12));
    }

    std::cout << "  -> reset() properly clears all accumulators and step counts." << std::endl;
}

void test_convex_quadratic_convergence() {
    std::cout << "[TEST] Running 2D convex quadratic convergence test (f(x,y) = x^2 + 4*y^2)..." << std::endl;

    auto objective = [](const std::vector<double>& p) {
        return p[0] * p[0] + 4.0 * p[1] * p[1];
    };
    auto compute_grad = [](const std::vector<double>& p) -> std::vector<double> {
        return {2.0 * p[0], 8.0 * p[1]};
    };

    struct TestConfig {
        std::unique_ptr<opt::Optimizer> opt;
        size_t max_steps;
        double target_loss;
    };

    std::vector<TestConfig> configs;
    configs.push_back({std::make_unique<opt::GradientDescent>(0.05), 150, 1e-3});
    configs.push_back({std::make_unique<opt::SGD>(0.05), 150, 1e-3});
    configs.push_back({std::make_unique<opt::MiniBatchSGD>(0.05, 4), 150, 1e-3});
    configs.push_back({std::make_unique<opt::Momentum>(0.02, 0.8), 150, 1e-3});
    configs.push_back({std::make_unique<opt::NAG>(0.02, 0.8), 150, 1e-3});
    configs.push_back({std::make_unique<opt::AdaGrad>(0.5), 300, 1e-2});
    configs.push_back({std::make_unique<opt::RMSProp>(0.05, 0.9), 150, 1e-3});
    configs.push_back({std::make_unique<opt::Adam>(0.1, 0.9, 0.999), 150, 1e-3});

    for (auto& cfg : configs) {
        std::vector<double> params = {4.0, 3.0};
        const double initial_loss = objective(params);

        for (size_t step = 0; step < cfg.max_steps; ++step) {
            auto grads = compute_grad(params);
            cfg.opt->step(params, grads);
            if (objective(params) < cfg.target_loss) {
                break;
            }
        }

        const double final_loss = objective(params);
        assert(final_loss < initial_loss);
        assert(final_loss < cfg.target_loss);
    }

    std::cout << "  -> All 8 optimizers converged successfully on convex quadratic." << std::endl;
}

void test_objectives_and_finite_differences() {
    std::cout << "[TEST] Running objective functions & finite-difference gradient verification..." << std::endl;

    std::vector<std::unique_ptr<opt::ObjectiveFunction>> objectives;
    objectives.push_back(std::make_unique<opt::ConvexQuadratic>());
    objectives.push_back(std::make_unique<opt::StronglyConvexQuadratic>());
    objectives.push_back(std::make_unique<opt::Rosenbrock>(1.0, 100.0));
    objectives.push_back(std::make_unique<opt::SaddlePoint>());
    objectives.push_back(std::make_unique<opt::Rastrigin>());
    objectives.push_back(std::make_unique<opt::Himmelblau>());

    // 1. Input validation on objective functions
    for (const auto& obj : objectives) {
        try {
            obj->value({1.0});
            assert(false && "Failed to throw on 1D input for 2D objective");
        } catch (const std::invalid_argument&) {}

        try {
            obj->gradient({1.0, 2.0, 3.0});
            assert(false && "Failed to throw on 3D input for 2D objective");
        } catch (const std::invalid_argument&) {}

        try {
            obj->value({std::numeric_limits<double>::quiet_NaN(), 1.0});
            assert(false && "Failed to throw on NaN input");
        } catch (const std::invalid_argument&) {}

        try {
            obj->gradient({1.0, std::numeric_limits<double>::infinity()});
            assert(false && "Failed to throw on Inf input");
        } catch (const std::invalid_argument&) {}
    }

    try {
        opt::Rosenbrock invalid_rosen(1.0, -5.0);
        assert(false && "Failed to throw on negative b parameter");
    } catch (const std::invalid_argument&) {}

    // 2. Test points spanning diverse regions
    const std::vector<std::vector<double>> test_points = {
        {0.0, 0.0},
        {1.2, -0.8},
        {-2.5, 3.1},
        {0.5, 0.5},
        {-1.7, -2.3},
        {3.0, 2.0}
    };

    const double fd_epsilon = 1e-6;
    const double tolerance = 1e-4;

    for (const auto& obj : objectives) {
        for (const auto& pt : test_points) {
            auto grad_analytic = obj->gradient(pt);
            auto grad_fd = obj->finite_difference_gradient(pt, fd_epsilon);

            assert(grad_analytic.size() == obj->dimension());
            assert(grad_fd.size() == obj->dimension());

            for (size_t d = 0; d < obj->dimension(); ++d) {
                const double diff = std::abs(grad_analytic[d] - grad_fd[d]);
                assert(diff <= tolerance);
            }
        }
    }

    // 3. Specific algebraic checks for key properties
    {
        opt::SaddlePoint saddle;
        assert(saddle.value({0.0, 0.0}) == 0.0);
        auto g = saddle.gradient({0.0, 0.0});
        assert(g[0] == 0.0 && g[1] == 0.0);
        assert(saddle.value({1.0, 0.0}) == 1.0);
        assert(saddle.value({0.0, 1.0}) == -1.0);
    }

    {
        opt::StronglyConvexQuadratic scq;
        assert(approx_equal(scq.condition_number(), 100.0));
        auto g1 = scq.gradient({1.0, 0.0});
        auto g2 = scq.gradient({0.0, 1.0});
        assert(approx_equal(g1[0], 1.0));
        assert(approx_equal(g2[1], 100.0));
    }

    {
        opt::Rosenbrock rosen;
        assert(approx_equal(rosen.value({1.0, 1.0}), 0.0));
        auto g = rosen.gradient({1.0, 1.0});
        assert(approx_equal(g[0], 0.0));
        assert(approx_equal(g[1], 0.0));
    }

    {
        opt::Rastrigin rast;
        assert(approx_equal(rast.value({0.0, 0.0}), 0.0));
        auto g = rast.gradient({0.0, 0.0});
        assert(approx_equal(g[0], 0.0));
        assert(approx_equal(g[1], 0.0));
    }

    {
        opt::Himmelblau himm;
        const std::vector<std::vector<double>> minima = {
            {3.0, 2.0},
            {-2.805118, 3.131312},
            {-3.779310, -3.283186},
            {3.584428, -1.848126}
        };
        for (const auto& m : minima) {
            assert(approx_equal(himm.value(m), 0.0, 1e-4));
            auto g = himm.gradient(m);
            assert(approx_equal(g[0], 0.0, 1e-3));
            assert(approx_equal(g[1], 0.0, 1e-3));
        }
    }

    std::cout << "  -> All 6 objective functions and finite-difference gradient checks passed." << std::endl;
}

void test_second_order_methods() {
    std::cout << "[TEST] Running second-order curvature methods & Gauss-Newton tests..." << std::endl;

    // 1. Pure Newton's method on StronglyConvexQuadratic (exact 1-step solve)
    {
        opt::StronglyConvexQuadratic scq;
        opt::NewtonOptimizer::Options opts;
        opts.lambda_reg = 0.0;
        opts.tolerance = 1e-12;
        opt::NewtonOptimizer newton(opts);

        auto val_fn = [&](const std::vector<double>& x) { return scq.value(x); };
        auto grad_fn = [&](const std::vector<double>& x) { return scq.gradient(x); };
        auto hess_fn = [](const std::vector<double>&) -> std::vector<std::vector<double>> {
            return {{1.0, 0.0}, {0.0, 100.0}};
        };

        auto res = newton.minimize(val_fn, grad_fn, hess_fn, {10.0, -5.0});
        assert(res.converged);
        assert(res.iterations <= 2);
        assert(approx_equal(res.point[0], 0.0, 1e-12));
        assert(approx_equal(res.point[1], 0.0, 1e-12));
        assert(approx_equal(res.final_loss, 0.0, 1e-14));
        std::cout << "    Newton quadratic 1-step convergence verified." << std::endl;
    }

    // 2. Newton's method on Rosenbrock from non-trivial start point (-1.2, 1.0)
    {
        opt::Rosenbrock rosen(1.0, 100.0);
        opt::NewtonOptimizer::Options opts;
        opts.max_iterations = 100;
        opts.lambda_reg = 1e-4;
        opts.tolerance = 1e-8;
        opt::NewtonOptimizer newton(opts);

        auto val_fn = [&](const std::vector<double>& x) { return rosen.value(x); };
        auto grad_fn = [&](const std::vector<double>& x) { return rosen.gradient(x); };
        auto hess_fn = [](const std::vector<double>& x) -> std::vector<std::vector<double>> {
            // f(x, y) = (1 - x)^2 + 100 * (y - x^2)^2
            // df/dx = 2*(x - 1) - 400*x*(y - x^2) = 2x - 2 - 400xy + 400x^3
            // df/dy = 200*(y - x^2)
            // d^2f/dx^2 = 2 - 400y + 1200x^2
            // d^2f/dxdy = -400x
            // d^2f/dy^2 = 200
            double H00 = 2.0 - 400.0 * x[1] + 1200.0 * x[0] * x[0];
            double H01 = -400.0 * x[0];
            double H10 = -400.0 * x[0];
            double H11 = 200.0;
            return {{H00, H01}, {H10, H11}};
        };

        auto res = newton.minimize(val_fn, grad_fn, hess_fn, {-1.2, 1.0});
        assert(res.converged);
        assert(approx_equal(res.point[0], 1.0, 1e-5));
        assert(approx_equal(res.point[1], 1.0, 1e-5));
        assert(res.final_loss < 1e-10);
        std::cout << "    Newton Rosenbrock convergence to (1, 1) verified." << std::endl;
    }

    // 3. Gauss-Newton nonlinear least-squares curve fit:
    // Model: y(x; a, b) = a * exp(-b * x)
    // Ground truth: a* = 2.5, b* = 1.3
    {
        const double a_true = 2.5;
        const double b_true = 1.3;

        std::vector<double> x_data = {0.0, 0.4, 0.8, 1.2, 1.6, 2.0, 2.5, 3.0};
        std::vector<double> y_data(x_data.size());
        for (size_t i = 0; i < x_data.size(); ++i) {
            y_data[i] = a_true * std::exp(-b_true * x_data[i]);
        }

        auto model_fn = [](double x, const std::vector<double>& theta) {
            return theta[0] * std::exp(-theta[1] * x);
        };
        auto jacobian_fn = [](double x, const std::vector<double>& theta) -> std::vector<double> {
            // dy/da = exp(-b * x)
            // dy/db = -a * x * exp(-b * x)
            double exp_bx = std::exp(-theta[1] * x);
            return {exp_bx, -theta[0] * x * exp_bx};
        };

        opt::GaussNewtonCurveFit gn;
        // Start from initial guess away from truth: (1.0, 0.5)
        auto res = gn.fit(x_data, y_data, model_fn, jacobian_fn, {1.0, 0.5});

        assert(res.converged);
        assert(approx_equal(res.params[0], a_true, 1e-5));
        assert(approx_equal(res.params[1], b_true, 1e-5));
        assert(res.sse < 1e-12);
        std::cout << "    Gauss-Newton curve fit recovered true parameters (a=2.5, b=1.3) with SSE < 1e-12." << std::endl;
    }

    // 4. Hessian-Vector Product (HVP) test against exact analytical matrix-vector product
    {
        // Test function: f(x, y) = x^3 + 2*x*y^2 + y^4
        // grad = [3*x^2 + 2*y^2, 4*x*y + 4*y^3]
        // H = [[6*x, 4*y], [4*y, 4*x + 12*y^2]]
        auto grad_fn = [](const std::vector<double>& x) -> std::vector<double> {
            return {3.0 * x[0] * x[0] + 2.0 * x[1] * x[1],
                    4.0 * x[0] * x[1] + 4.0 * std::pow(x[1], 3)};
        };

        const std::vector<double> x_eval = {1.5, -2.0};
        const std::vector<double> v_dir = {0.7, -1.3};

        // Exact analytical Hessian * v:
        // H00 = 6*1.5 = 9.0, H01 = 4*(-2) = -8.0
        // H10 = -8.0, H11 = 4*1.5 + 12*(-2)^2 = 6 + 48 = 54.0
        // (H*v)_0 = 9*0.7 + (-8)*(-1.3) = 6.3 + 10.4 = 16.7
        // (H*v)_1 = (-8)*0.7 + 54*(-1.3) = -5.6 - 70.2 = -75.8
        const double expected_hvp0 = 16.7;
        const double expected_hvp1 = -75.8;

        auto hvp_num = opt::hessian_vector_product(grad_fn, x_eval, v_dir, 1e-5);
        assert(approx_equal(hvp_num[0], expected_hvp0, 1e-3));
        assert(approx_equal(hvp_num[1], expected_hvp1, 1e-3));
        std::cout << "    Hessian-Vector Product matches exact analytical Hv within 1e-3." << std::endl;
    }

    // 5. Diagonal Fisher Information Matrix approximation
    {
        // 3 samples with gradients:
        // g_1 = [2.0, -1.0], g_2 = [-3.0, 0.5], g_3 = [1.0, 2.0]
        // F0 = (4 + 9 + 1) / 3 = 14 / 3 = 4.666667
        // F1 = (1 + 0.25 + 4) / 3 = 5.25 / 3 = 1.75
        std::vector<std::vector<double>> per_sample_grads = {
            {2.0, -1.0},
            {-3.0, 0.5},
            {1.0, 2.0}
        };
        auto diag_fisher = opt::compute_diagonal_fisher(per_sample_grads);
        assert(approx_equal(diag_fisher[0], 14.0 / 3.0, 1e-12));
        assert(approx_equal(diag_fisher[1], 1.75, 1e-12));
        std::cout << "    Diagonal Fisher approximation matches analytical expectation." << std::endl;
    }

    // 6. Low-Rank Curvature Approximation (L-BFGS secant condition)
    {
        // Consider positive definite quadratic f(x) = 0.5 * x^T A x with A = [[4, 1], [1, 3]]
        // grad(x) = A * x
        auto mat_vec = [](const std::vector<double>& x) -> std::vector<double> {
            return {4.0 * x[0] + 1.0 * x[1], 1.0 * x[0] + 3.0 * x[1]};
        };

        const std::vector<double> s = {1.0, 2.0};
        const std::vector<double> y = mat_vec(s); // y = [4(1)+1(2), 1(1)+3(2)] = [6.0, 7.0]

        opt::LowRankCurvatureLBFGS lbfgs(5);
        lbfgs.update(s, y);

        // By secant condition: H_k * y = s
        auto reconstructed_s = lbfgs.multiply_inverse_hessian(y);
        assert(approx_equal(reconstructed_s[0], s[0], 1e-5));
        assert(approx_equal(reconstructed_s[1], s[1], 1e-5));
        std::cout << "    Low-Rank Curvature L-BFGS satisfies secant condition H_k * y = s." << std::endl;
    }

    std::cout << "  -> All second-order methods and curvature approximations verified." << std::endl;
}

} // namespace

int main() {
    std::cout << "====================================================" << std::endl;
    std::cout << " Running Track A Unit Tests: First & Second-Order Methods" << std::endl;
    std::cout << "====================================================" << std::endl;

    try {
        test_input_validation();
        test_analytical_single_step();
        test_reset_behavior();
        test_convex_quadratic_convergence();
        test_objectives_and_finite_differences();
        test_second_order_methods();
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Unhandled exception in unit test: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "====================================================" << std::endl;
    std::cout << " All Track A Optimizer, Objective & Second-Order Tests Passed (100% OK)." << std::endl;
    std::cout << "====================================================" << std::endl;
    return 0;
}
