/**
 * @file test_main.cpp
 * @brief Comprehensive unit tests for continuous optimizers, benchmark objectives,
 *        second-order curvature methods, and neural network backpropagation.
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
 *  7. Two-Layer MLP Hand-Written Backpropagation:
 *     - Analytical backprop gradients verified against finite differences (< 1e-5).
 *     - Full training and loss reduction across all 8 optimizers from identical initialization.
 *     - Clear separation between gradient computation (backprop) and parameter update (optimizer).
 *
 * Optimization Techniques | Lab Assignment 8 (Track A)
 */

#include "optimizers.hpp"
#include "objectives.hpp"
#include "second_order.hpp"
#include "neural_net.hpp"
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

    try {
        opt::GradientDescent gd(-0.01);
        assert(false && "Failed to throw on negative learning rate");
    } catch (const std::invalid_argument&) {}

    try {
        opt::SGD sgd(0.0);
        assert(false && "Failed to throw on zero learning rate");
    } catch (const std::invalid_argument&) {}

    try {
        opt::MiniBatchSGD mb(0.01, 0);
        assert(false && "Failed to throw on zero batch size");
    } catch (const std::invalid_argument&) {}

    try {
        opt::Momentum mom(0.01, 1.0);
        assert(false && "Failed to throw on beta >= 1.0");
    } catch (const std::invalid_argument&) {}

    try {
        opt::NAG nag(0.01, -0.1);
        assert(false && "Failed to throw on negative beta");
    } catch (const std::invalid_argument&) {}

    try {
        opt::AdaGrad ada(0.01, 0.0);
        assert(false && "Failed to throw on non-positive epsilon");
    } catch (const std::invalid_argument&) {}

    try {
        opt::Adam adam(0.01, 0.9, 0.999, -1e-5);
        assert(false && "Failed to throw on negative epsilon");
    } catch (const std::invalid_argument&) {}

    opt::GradientDescent gd(0.1);
    std::vector<double> params = {1.0, 2.0};
    std::vector<double> grads_mismatch = {0.1};
    try {
        gd.step(params, grads_mismatch);
        assert(false && "Failed to throw on dimension mismatch");
    } catch (const std::invalid_argument&) {}

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

    {
        opt::GradientDescent gd(0.1);
        auto p = initial_params;
        gd.step(p, grads);
        assert(approx_equal(p[0], 0.95));
        assert(approx_equal(p[1], 2.025));
    }

    {
        opt::SGD sgd(0.1);
        auto p = initial_params;
        sgd.step(p, grads);
        assert(approx_equal(p[0], 0.95));
        assert(approx_equal(p[1], 2.025));
    }

    {
        opt::MiniBatchSGD mb(0.1, 4);
        auto p = initial_params;
        mb.step(p, grads);
        assert(approx_equal(p[0], 0.95));
        assert(approx_equal(p[1], 2.025));
    }

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

    {
        opt::NAG nag(0.1, 0.9);
        auto p = initial_params;
        nag.step(p, grads);
        assert(approx_equal(p[0], 0.905));
        assert(approx_equal(p[1], 2.0475));
    }

    {
        opt::AdaGrad ada(0.1, 1e-8);
        auto p = initial_params;
        ada.step(p, grads);
        assert(approx_equal(p[0], 0.90));
        assert(approx_equal(p[1], 2.10));
    }

    {
        opt::RMSProp rmsprop(0.1, 0.9, 1e-8);
        auto p = initial_params;
        rmsprop.step(p, grads);
        double expected_v0 = 0.1 * 0.25;
        double expected_p0 = 1.0 - (0.1 / (std::sqrt(expected_v0) + 1e-8)) * 0.5;
        assert(approx_equal(p[0], expected_p0));
    }

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
            double exp_bx = std::exp(-theta[1] * x);
            return {exp_bx, -theta[0] * x * exp_bx};
        };

        opt::GaussNewtonCurveFit gn;
        auto res = gn.fit(x_data, y_data, model_fn, jacobian_fn, {1.0, 0.5});

        assert(res.converged);
        assert(approx_equal(res.params[0], a_true, 1e-5));
        assert(approx_equal(res.params[1], b_true, 1e-5));
        assert(res.sse < 1e-12);
        std::cout << "    Gauss-Newton curve fit recovered true parameters (a=2.5, b=1.3) with SSE < 1e-12." << std::endl;
    }

    // 4. Hessian-Vector Product (HVP) test against exact analytical matrix-vector product
    {
        auto grad_fn = [](const std::vector<double>& x) -> std::vector<double> {
            return {3.0 * x[0] * x[0] + 2.0 * x[1] * x[1],
                    4.0 * x[0] * x[1] + 4.0 * std::pow(x[1], 3)};
        };

        const std::vector<double> x_eval = {1.5, -2.0};
        const std::vector<double> v_dir = {0.7, -1.3};
        const double expected_hvp0 = 16.7;
        const double expected_hvp1 = -75.8;

        auto hvp_num = opt::hessian_vector_product(grad_fn, x_eval, v_dir, 1e-5);
        assert(approx_equal(hvp_num[0], expected_hvp0, 1e-3));
        assert(approx_equal(hvp_num[1], expected_hvp1, 1e-3));
        std::cout << "    Hessian-Vector Product matches exact analytical Hv within 1e-3." << std::endl;
    }

    // 5. Diagonal Fisher Information Matrix approximation
    {
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
        auto mat_vec = [](const std::vector<double>& x) -> std::vector<double> {
            return {4.0 * x[0] + 1.0 * x[1], 1.0 * x[0] + 3.0 * x[1]};
        };

        const std::vector<double> s = {1.0, 2.0};
        const std::vector<double> y = mat_vec(s);

        opt::LowRankCurvatureLBFGS lbfgs(5);
        lbfgs.update(s, y);

        auto reconstructed_s = lbfgs.multiply_inverse_hessian(y);
        assert(approx_equal(reconstructed_s[0], s[0], 1e-5));
        assert(approx_equal(reconstructed_s[1], s[1], 1e-5));
        std::cout << "    Low-Rank Curvature L-BFGS satisfies secant condition H_k * y = s." << std::endl;
    }

    std::cout << "  -> All second-order methods and curvature approximations verified." << std::endl;
}

void test_neural_network_backprop() {
    std::cout << "[TEST] Running Two-Layer MLP Hand-Written Backpropagation & Optimizer Training..." << std::endl;

    // Small non-linear dataset (4 samples of XOR function)
    const std::vector<std::vector<double>> X = {
        {0.0, 0.0},
        {0.0, 1.0},
        {1.0, 0.0},
        {1.0, 1.0}
    };
    const std::vector<std::vector<double>> Y = {
        {0.0},
        {1.0},
        {1.0},
        {0.0}
    };

    opt::TwoLayerMLP mlp(2, 4, 1, 42);
    // W1: 4*2=8, b1: 4, W2: 1*4=4, b2: 1 => Total 17 parameters
    assert(mlp.total_parameters() == 17);

    // 1. Verify analytical backpropagation gradients against central finite differences
    auto backprop_grads = mlp.compute_gradients(X, Y);
    auto fd_grads = mlp.finite_difference_gradients(X, Y, 1e-6);

    assert(backprop_grads.size() == fd_grads.size());
    assert(backprop_grads.size() == mlp.total_parameters());

    for (size_t i = 0; i < backprop_grads.size(); ++i) {
        double diff = std::abs(backprop_grads[i] - fd_grads[i]);
        assert(diff < 1e-5);
    }
    std::cout << "  -> Hand-written backpropagation verified against finite differences (diff < 1e-5 on all 17 params)." << std::endl;

    // 2. Train the MLP on this dataset using all 8 optimizers
    // Save initial parameters to ensure fair comparison from identical starting point
    const auto initial_params = mlp.get_parameters();
    const double initial_loss = mlp.compute_loss(X, Y);

    struct OptTestConfig {
        std::unique_ptr<opt::Optimizer> opt;
        size_t epochs;
    };

    std::vector<OptTestConfig> opt_configs;
    opt_configs.push_back({std::make_unique<opt::GradientDescent>(0.4), 300});
    opt_configs.push_back({std::make_unique<opt::SGD>(0.4), 300});
    opt_configs.push_back({std::make_unique<opt::MiniBatchSGD>(0.4, 2), 300});
    opt_configs.push_back({std::make_unique<opt::Momentum>(0.1, 0.8), 300});
    opt_configs.push_back({std::make_unique<opt::NAG>(0.1, 0.8), 300});
    opt_configs.push_back({std::make_unique<opt::AdaGrad>(0.5), 300});
    opt_configs.push_back({std::make_unique<opt::RMSProp>(0.03, 0.9), 300});
    opt_configs.push_back({std::make_unique<opt::Adam>(0.05, 0.9, 0.999), 300});

    for (auto& cfg : opt_configs) {
        mlp.set_parameters(initial_params);
        cfg.opt->reset();

        for (size_t epoch = 0; epoch < cfg.epochs; ++epoch) {
            auto current_params = mlp.get_parameters();
            auto grads = mlp.compute_gradients(X, Y);
            cfg.opt->step(current_params, grads);
            mlp.set_parameters(current_params);
        }

        const double final_loss = mlp.compute_loss(X, Y);
        assert(final_loss < initial_loss);
        std::cout << "    " << std::setw(16) << std::left << cfg.opt->name()
                  << " initial loss: " << std::fixed << std::setprecision(5) << initial_loss
                  << " -> final loss: " << final_loss
                  << " (loss reduced by " << ((initial_loss - final_loss) / initial_loss) * 100.0 << "%)"
                  << std::endl;
    }

    std::cout << "  -> All 8 optimizers successfully trained the MLP with hand-written backpropagation." << std::endl;
}

} // namespace

int main() {
    std::cout << "====================================================" << std::endl;
    std::cout << " Running Track A Unit Tests: Optimizers, Objectives, Curvature & Backprop" << std::endl;
    std::cout << "====================================================" << std::endl;

    try {
        test_input_validation();
        test_analytical_single_step();
        test_reset_behavior();
        test_convex_quadratic_convergence();
        test_objectives_and_finite_differences();
        test_second_order_methods();
        test_neural_network_backprop();
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Unhandled exception in unit test: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "====================================================" << std::endl;
    std::cout << " All Track A Unit & Integration Tests Passed (100% OK)." << std::endl;
    std::cout << "====================================================" << std::endl;
    return 0;
}
