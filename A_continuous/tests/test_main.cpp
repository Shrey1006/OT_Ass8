/**
 * @file test_main.cpp
 * @brief Comprehensive unit tests for continuous optimizers.
 *
 * Verifies:
 *  1. Input validation (dimensions, bounds, non-finite values).
 *  2. Analytical correctness against hand-calculated values.
 *  3. Reset behavior restoring clean initial state.
 *  4. Convergence on a 2D convex quadratic surface.
 *  5. Deterministic reproducibility.
 *
 * Optimization Techniques | Lab Assignment 8 (Track A)
 */

#include "optimizers.hpp"
#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>
#include <iomanip>
#include <memory>
#include <limits>

namespace {

bool approx_equal(double a, double b, double tol = 1e-6) {
    return std::abs(a - b) <= tol;
}

void test_input_validation() {
    std::cout << "[TEST] Running input validation tests..." << std::endl;

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

    std::cout << "  -> Input validation passed successfully." << std::endl;
}

void test_analytical_single_step() {
    std::cout << "[TEST] Running analytical single-step numerical verification..." << std::endl;

    const std::vector<double> initial_params = {1.0, 2.0};
    const std::vector<double> grads = {0.5, -0.25};

    // 1. GradientDescent: theta = theta - lr * grads = [1.0 - 0.1*0.5, 2.0 - 0.1*(-0.25)] = [0.95, 2.025]
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

    // 4. Momentum (Heavy Ball):
    // v_1 = 0.9 * 0 + 0.1 * [0.5, -0.25] = [0.05, -0.025]
    // p_1 = [1.0, 2.0] - [0.05, -0.025] = [0.95, 2.025]
    {
        opt::Momentum mom(0.1, 0.9);
        auto p = initial_params;
        mom.step(p, grads);
        assert(approx_equal(p[0], 0.95));
        assert(approx_equal(p[1], 2.025));

        // Step 2 with new grad [0.1, 0.2]:
        // v_2[0] = 0.9 * 0.05 + 0.1 * 0.1 = 0.045 + 0.01 = 0.055
        // p_2[0] = 0.95 - 0.055 = 0.895
        std::vector<double> g2 = {0.1, 0.2};
        mom.step(p, g2);
        assert(approx_equal(p[0], 0.895));
    }

    // 5. NAG:
    // v_1 = 0.9 * 0 + [0.5, -0.25] = [0.5, -0.25]
    // p_1[0] = 1.0 - 0.1 * (0.5 + 0.9 * 0.5) = 1.0 - 0.1 * (0.95) = 0.905
    // p_1[1] = 2.0 - 0.1 * (-0.25 + 0.9 * (-0.25)) = 2.0 - 0.1 * (-0.475) = 2.0475
    {
        opt::NAG nag(0.1, 0.9);
        auto p = initial_params;
        nag.step(p, grads);
        assert(approx_equal(p[0], 0.905));
        assert(approx_equal(p[1], 2.0475));
    }

    // 6. AdaGrad:
    // G_1[0] = 0.5^2 = 0.25
    // p_1[0] = 1.0 - (0.1 / (sqrt(0.25) + 1e-8)) * 0.5 = 1.0 - (0.1 / 0.5) * 0.5 = 0.9
    // G_1[1] = (-0.25)^2 = 0.0625
    // p_1[1] = 2.0 - (0.1 / (sqrt(0.0625) + 1e-8)) * (-0.25) = 2.0 - (0.1 / 0.25) * (-0.25) = 2.1
    {
        opt::AdaGrad ada(0.1, 1e-8);
        auto p = initial_params;
        ada.step(p, grads);
        assert(approx_equal(p[0], 0.90));
        assert(approx_equal(p[1], 2.10));
    }

    // 7. RMSProp:
    // v_1[0] = 0.9 * 0 + 0.1 * (0.5^2) = 0.025
    // p_1[0] = 1.0 - (0.1 / (sqrt(0.025) + 1e-8)) * 0.5
    {
        opt::RMSProp rmsprop(0.1, 0.9, 1e-8);
        auto p = initial_params;
        rmsprop.step(p, grads);
        double expected_v0 = 0.1 * 0.25;
        double expected_p0 = 1.0 - (0.1 / (std::sqrt(expected_v0) + 1e-8)) * 0.5;
        assert(approx_equal(p[0], expected_p0));
    }

    // 8. Adam:
    // At t=1:
    // m_1[0] = (1 - 0.9) * 0.5 = 0.05 => m_hat = 0.05 / (1 - 0.9) = 0.5
    // v_1[0] = (1 - 0.999) * 0.25 = 0.00025 => v_hat = 0.00025 / (1 - 0.999) = 0.25
    // p_1[0] = 1.0 - (0.1 / (sqrt(0.25) + 1e-8)) * 0.5 = 1.0 - 0.1 = 0.9
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
        // Step 1
        auto p_run1 = p0;
        opt->step(p_run1, g0);
        auto first_step_result = p_run1;

        // Step 2
        opt->step(p_run1, g1);

        // Reset
        opt->reset();
        assert(opt->get_step_count() == 0);

        // Step 1 again after reset must match initial step exactly
        auto p_run2 = p0;
        opt->step(p_run2, g0);
        assert(approx_equal(p_run2[0], first_step_result[0], 1e-12));
        assert(approx_equal(p_run2[1], first_step_result[1], 1e-12));
    }

    std::cout << "  -> reset() properly clears all accumulators and step counts." << std::endl;
}

void test_convex_quadratic_convergence() {
    std::cout << "[TEST] Running 2D convex quadratic convergence test (f(x,y) = x^2 + 4*y^2)..." << std::endl;

    // Objective: f(x, y) = x^2 + 4*y^2
    // Minimum at (0, 0), f(0, 0) = 0.
    // Gradients: df/dx = 2*x, df/dy = 8*y.

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
        std::cout << "    " << std::setw(16) << std::left << cfg.opt->name()
                  << " converged from " << initial_loss << " to " << final_loss
                  << " in <= " << cfg.max_steps << " steps." << std::endl;
    }

    std::cout << "  -> All 8 optimizers converged successfully on convex quadratic." << std::endl;
}

} // namespace

int main() {
    std::cout << "====================================================" << std::endl;
    std::cout << " Running Track A Unit Tests: optimizers.hpp" << std::endl;
    std::cout << "====================================================" << std::endl;

    try {
        test_input_validation();
        test_analytical_single_step();
        test_reset_behavior();
        test_convex_quadratic_convergence();
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Unhandled exception in unit test: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "====================================================" << std::endl;
    std::cout << " All Track A Optimizer Unit Tests Passed (100% OK)." << std::endl;
    std::cout << "====================================================" << std::endl;
    return 0;
}
