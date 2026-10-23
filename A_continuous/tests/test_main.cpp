/**
 * @file test_main.cpp
 * @brief Comprehensive unit tests for continuous optimizers and benchmark objectives.
 *
 * Verifies:
 *  1. Input validation (dimensions, bounds, non-finite values) for optimizers & objectives.
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
 *
 * Optimization Techniques | Lab Assignment 8 (Track A)
 */

#include "optimizers.hpp"
#include "objectives.hpp"
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
        // Dimension mismatch checks
        try {
            obj->value({1.0});
            assert(false && "Failed to throw on 1D input for 2D objective");
        } catch (const std::invalid_argument&) {}

        try {
            obj->gradient({1.0, 2.0, 3.0});
            assert(false && "Failed to throw on 3D input for 2D objective");
        } catch (const std::invalid_argument&) {}

        // Non-finite input checks
        try {
            obj->value({std::numeric_limits<double>::quiet_NaN(), 1.0});
            assert(false && "Failed to throw on NaN input");
        } catch (const std::invalid_argument&) {}

        try {
            obj->gradient({1.0, std::numeric_limits<double>::infinity()});
            assert(false && "Failed to throw on Inf input");
        } catch (const std::invalid_argument&) {}
    }

    // Invalid parameters for Rosenbrock
    try {
        opt::Rosenbrock invalid_rosen(1.0, -5.0);
        assert(false && "Failed to throw on negative b parameter");
    } catch (const std::invalid_argument&) {}

    // 2. Test points spanning diverse regions: origin, positive, negative, mixed
    const std::vector<std::vector<double>> test_points = {
        {0.0, 0.0},
        {1.2, -0.8},
        {-2.5, 3.1},
        {0.5, 0.5},
        {-1.7, -2.3},
        {3.0, 2.0}
    };

    const double fd_epsilon = 1e-6;
    const double tolerance = 1e-4; // Central difference error is O(h^2) = 1e-12 + rounding

    for (const auto& obj : objectives) {
        std::cout << "  -> Verifying analytic vs finite-differences for: " << obj->name() << std::endl;
        for (const auto& pt : test_points) {
            auto grad_analytic = obj->gradient(pt);
            auto grad_fd = obj->finite_difference_gradient(pt, fd_epsilon);

            assert(grad_analytic.size() == obj->dimension());
            assert(grad_fd.size() == obj->dimension());

            for (size_t d = 0; d < obj->dimension(); ++d) {
                const double diff = std::abs(grad_analytic[d] - grad_fd[d]);
                if (diff > tolerance) {
                    std::cerr << "Mismatch on " << obj->name() << " at (" << pt[0] << ", " << pt[1]
                              << ") dim " << d << ": analytic=" << grad_analytic[d]
                              << " vs fd=" << grad_fd[d] << " (diff=" << diff << ")" << std::endl;
                    assert(false);
                }
            }
        }
    }

    // 3. Specific algebraic checks for key properties
    // SaddlePoint: f(0,0)=0, gradient=(0,0) is a saddle point
    {
        opt::SaddlePoint saddle;
        assert(saddle.value({0.0, 0.0}) == 0.0);
        auto g = saddle.gradient({0.0, 0.0});
        assert(g[0] == 0.0 && g[1] == 0.0);
        // Convex along x: f(1, 0) = 1 > 0
        assert(saddle.value({1.0, 0.0}) == 1.0);
        // Concave along y: f(0, 1) = -1 < 0
        assert(saddle.value({0.0, 1.0}) == -1.0);
    }

    // StronglyConvexQuadratic condition number
    {
        opt::StronglyConvexQuadratic scq;
        assert(approx_equal(scq.condition_number(), 100.0));
        // Hessian is diag(1, 100)
        auto g1 = scq.gradient({1.0, 0.0});
        auto g2 = scq.gradient({0.0, 1.0});
        assert(approx_equal(g1[0], 1.0));
        assert(approx_equal(g2[1], 100.0));
    }

    // Rosenbrock global minimum at (1, 1) where f(1, 1) = 0
    {
        opt::Rosenbrock rosen;
        assert(approx_equal(rosen.value({1.0, 1.0}), 0.0));
        auto g = rosen.gradient({1.0, 1.0});
        assert(approx_equal(g[0], 0.0));
        assert(approx_equal(g[1], 0.0));
    }

    // Rastrigin global minimum at (0, 0) where f(0, 0) = 0
    {
        opt::Rastrigin rast;
        assert(approx_equal(rast.value({0.0, 0.0}), 0.0));
        auto g = rast.gradient({0.0, 0.0});
        assert(approx_equal(g[0], 0.0));
        assert(approx_equal(g[1], 0.0));
    }

    // Himmelblau: all 4 global minima evaluate to 0
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

} // namespace

int main() {
    std::cout << "====================================================" << std::endl;
    std::cout << " Running Track A Unit Tests: optimizers & objectives" << std::endl;
    std::cout << "====================================================" << std::endl;

    try {
        test_input_validation();
        test_analytical_single_step();
        test_reset_behavior();
        test_convex_quadratic_convergence();
        test_objectives_and_finite_differences();
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Unhandled exception in unit test: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "====================================================" << std::endl;
    std::cout << " All Track A Optimizer & Objective Tests Passed (100% OK)." << std::endl;
    std::cout << "====================================================" << std::endl;
    return 0;
}
