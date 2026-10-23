/**
 * @file optimizers.hpp
 * @brief First-order continuous optimization algorithms in C++17.
 *
 * Implements:
 *  - Gradient Descent (GD)
 *  - Stochastic Gradient Descent (SGD)
 *  - Mini-Batch SGD
 *  - Polyak Momentum (Heavy Ball)
 *  - Nesterov Accelerated Gradient (NAG)
 *  - AdaGrad
 *  - RMSProp
 *  - Adam (Adaptive Moment Estimation)
 *
 * All optimizers share a common polymorphic interface (Optimizer) featuring
 * step() for in-place parameter updates and reset() for clearing internal state.
 * Fixed random seeds and strict input validation are enforced throughout.
 *
 * Optimization Techniques | Lab Assignment 8 (Track A)
 */

#ifndef OPTIMIZERS_HPP
#define OPTIMIZERS_HPP

#include <vector>
#include <cmath>
#include <stdexcept>
#include <string>
#include <sstream>
#include <cstddef>

namespace opt {

/**
 * @brief Base abstract class for continuous optimizers.
 */
class Optimizer {
public:
    virtual ~Optimizer() = default;

    /**
     * @brief Performs one optimization update step on parameters in-place.
     * @param params Vector of parameters to update.
     * @param grads Gradient vector evaluated at the parameters (dLoss/dParams).
     * @throws std::invalid_argument If dimensions mismatch or if any value is non-finite.
     */
    virtual void step(std::vector<double>& params, const std::vector<double>& grads) = 0;

    /**
     * @brief Resets all internal optimizer state (accumulators, momentum, timestep).
     */
    virtual void reset() = 0;

    /**
     * @brief Returns the name of the optimization algorithm.
     */
    virtual const char* name() const noexcept = 0;

    /**
     * @brief Current step count since initialization or last reset.
     */
    size_t get_step_count() const noexcept { return step_count_; }

protected:
    size_t step_count_ = 0;

    /**
     * @brief Validates parameter and gradient vectors for dimension match and finiteness.
     */
    static void validate_inputs(const std::vector<double>& params, const std::vector<double>& grads) {
        if (params.size() != grads.size()) {
            std::ostringstream oss;
            oss << "Dimension mismatch: params size (" << params.size()
                << ") != grads size (" << grads.size() << ")";
            throw std::invalid_argument(oss.str());
        }
        for (size_t i = 0; i < params.size(); ++i) {
            if (!std::isfinite(params[i])) {
                throw std::invalid_argument("Non-finite value detected in params at index " + std::to_string(i));
            }
            if (!std::isfinite(grads[i])) {
                throw std::invalid_argument("Non-finite value detected in grads at index " + std::to_string(i));
            }
        }
    }
};

/**
 * @brief Standard (Batch) Gradient Descent (GD).
 *
 * Update rule:
 *   \theta_{t+1} = \theta_t - \eta * g_t
 */
class GradientDescent : public Optimizer {
public:
    explicit GradientDescent(double learning_rate)
        : lr_(learning_rate) {
        if (lr_ <= 0.0 || !std::isfinite(lr_)) {
            throw std::invalid_argument("GradientDescent: learning_rate must be positive and finite.");
        }
    }

    void step(std::vector<double>& params, const std::vector<double>& grads) override {
        validate_inputs(params, grads);
        for (size_t i = 0; i < params.size(); ++i) {
            params[i] -= lr_ * grads[i];
        }
        ++step_count_;
    }

    void reset() override {
        step_count_ = 0;
    }

    const char* name() const noexcept override {
        return "GradientDescent";
    }

    double get_learning_rate() const noexcept { return lr_; }

private:
    double lr_;
};

/**
 * @brief Stochastic Gradient Descent (SGD).
 *
 * Update rule:
 *   \theta_{t+1} = \theta_t - \eta * g_t
 * Where g_t represents a stochastic gradient sample.
 */
class SGD : public Optimizer {
public:
    explicit SGD(double learning_rate)
        : lr_(learning_rate) {
        if (lr_ <= 0.0 || !std::isfinite(lr_)) {
            throw std::invalid_argument("SGD: learning_rate must be positive and finite.");
        }
    }

    void step(std::vector<double>& params, const std::vector<double>& grads) override {
        validate_inputs(params, grads);
        for (size_t i = 0; i < params.size(); ++i) {
            params[i] -= lr_ * grads[i];
        }
        ++step_count_;
    }

    void reset() override {
        step_count_ = 0;
    }

    const char* name() const noexcept override {
        return "SGD";
    }

    double get_learning_rate() const noexcept { return lr_; }

private:
    double lr_;
};

/**
 * @brief Mini-Batch Stochastic Gradient Descent.
 *
 * Validates batch size B >= 1. The input gradient vector is expected to be the
 * batch-averaged gradient (or accumulated gradient).
 *
 * Update rule:
 *   \theta_{t+1} = \theta_t - \eta * g_t
 */
class MiniBatchSGD : public Optimizer {
public:
    MiniBatchSGD(double learning_rate, size_t batch_size)
        : lr_(learning_rate), batch_size_(batch_size) {
        if (lr_ <= 0.0 || !std::isfinite(lr_)) {
            throw std::invalid_argument("MiniBatchSGD: learning_rate must be positive and finite.");
        }
        if (batch_size_ == 0) {
            throw std::invalid_argument("MiniBatchSGD: batch_size must be >= 1.");
        }
    }

    void step(std::vector<double>& params, const std::vector<double>& grads) override {
        validate_inputs(params, grads);
        for (size_t i = 0; i < params.size(); ++i) {
            params[i] -= lr_ * grads[i];
        }
        ++step_count_;
    }

    void reset() override {
        step_count_ = 0;
    }

    const char* name() const noexcept override {
        return "MiniBatchSGD";
    }

    size_t get_batch_size() const noexcept { return batch_size_; }
    double get_learning_rate() const noexcept { return lr_; }

private:
    double lr_;
    size_t batch_size_;
};

/**
 * @brief Polyak Momentum (Classical Heavy Ball method).
 *
 * Accelerates gradient descent by accumulating velocity in directions of persistent gradient:
 *   v_t = \beta * v_{t-1} + \eta * g_t
 *   \theta_t = \theta_{t-1} - v_t
 */
class Momentum : public Optimizer {
public:
    Momentum(double learning_rate, double beta = 0.9)
        : lr_(learning_rate), beta_(beta) {
        if (lr_ <= 0.0 || !std::isfinite(lr_)) {
            throw std::invalid_argument("Momentum: learning_rate must be positive and finite.");
        }
        if (beta_ < 0.0 || beta_ >= 1.0 || !std::isfinite(beta_)) {
            throw std::invalid_argument("Momentum: beta must satisfy 0.0 <= beta < 1.0.");
        }
    }

    void step(std::vector<double>& params, const std::vector<double>& grads) override {
        validate_inputs(params, grads);

        if (velocity_.size() != params.size()) {
            velocity_.assign(params.size(), 0.0);
        }

        for (size_t i = 0; i < params.size(); ++i) {
            velocity_[i] = beta_ * velocity_[i] + lr_ * grads[i];
            params[i] -= velocity_[i];
        }
        ++step_count_;
    }

    void reset() override {
        velocity_.clear();
        step_count_ = 0;
    }

    const char* name() const noexcept override {
        return "Momentum";
    }

    const std::vector<double>& get_velocity() const noexcept { return velocity_; }

private:
    double lr_;
    double beta_;
    std::vector<double> velocity_;
};

/**
 * @brief Nesterov Accelerated Gradient (NAG).
 *
 * Implements Nesterov's accelerated momentum formulation:
 *   v_t = \beta * v_{t-1} + g_t
 *   \theta_t = \theta_{t-1} - \eta * (g_t + \beta * v_t)
 *
 * This lookahead correction anticipates future parameter position before applying
 * the damping momentum step.
 */
class NAG : public Optimizer {
public:
    NAG(double learning_rate, double beta = 0.9)
        : lr_(learning_rate), beta_(beta) {
        if (lr_ <= 0.0 || !std::isfinite(lr_)) {
            throw std::invalid_argument("NAG: learning_rate must be positive and finite.");
        }
        if (beta_ < 0.0 || beta_ >= 1.0 || !std::isfinite(beta_)) {
            throw std::invalid_argument("NAG: beta must satisfy 0.0 <= beta < 1.0.");
        }
    }

    void step(std::vector<double>& params, const std::vector<double>& grads) override {
        validate_inputs(params, grads);

        if (velocity_.size() != params.size()) {
            velocity_.assign(params.size(), 0.0);
        }

        for (size_t i = 0; i < params.size(); ++i) {
            velocity_[i] = beta_ * velocity_[i] + grads[i];
            params[i] -= lr_ * (grads[i] + beta_ * velocity_[i]);
        }
        ++step_count_;
    }

    void reset() override {
        velocity_.clear();
        step_count_ = 0;
    }

    const char* name() const noexcept override {
        return "NAG";
    }

    const std::vector<double>& get_velocity() const noexcept { return velocity_; }

private:
    double lr_;
    double beta_;
    std::vector<double> velocity_;
};

/**
 * @brief AdaGrad (Adaptive Gradient Algorithm).
 *
 * Accumulates historical sum of squared gradients coordinate-wise:
 *   G_t = G_{t-1} + g_t^2
 *   \theta_t = \theta_{t-1} - \frac{\eta}{\sqrt{G_t} + \epsilon} * g_t
 */
class AdaGrad : public Optimizer {
public:
    explicit AdaGrad(double learning_rate, double epsilon = 1e-8)
        : lr_(learning_rate), eps_(epsilon) {
        if (lr_ <= 0.0 || !std::isfinite(lr_)) {
            throw std::invalid_argument("AdaGrad: learning_rate must be positive and finite.");
        }
        if (eps_ <= 0.0 || !std::isfinite(eps_)) {
            throw std::invalid_argument("AdaGrad: epsilon must be strictly positive and finite.");
        }
    }

    void step(std::vector<double>& params, const std::vector<double>& grads) override {
        validate_inputs(params, grads);

        if (g_squared_.size() != params.size()) {
            g_squared_.assign(params.size(), 0.0);
        }

        for (size_t i = 0; i < params.size(); ++i) {
            g_squared_[i] += grads[i] * grads[i];
            params[i] -= (lr_ / (std::sqrt(g_squared_[i]) + eps_)) * grads[i];
        }
        ++step_count_;
    }

    void reset() override {
        g_squared_.clear();
        step_count_ = 0;
    }

    const char* name() const noexcept override {
        return "AdaGrad";
    }

    const std::vector<double>& get_accumulated_grads() const noexcept { return g_squared_; }

private:
    double lr_;
    double eps_;
    std::vector<double> g_squared_;
};

/**
 * @brief RMSProp (Root Mean Square Propagation).
 *
 * Utilizes an exponential moving average of squared gradients to resolve
 * AdaGrad's diminishing learning rate problem on non-convex landscapes:
 *   v_t = \beta * v_{t-1} + (1 - \beta) * g_t^2
 *   \theta_t = \theta_{t-1} - \frac{\eta}{\sqrt{v_t} + \epsilon} * g_t
 */
class RMSProp : public Optimizer {
public:
    explicit RMSProp(double learning_rate, double decay_rate = 0.9, double epsilon = 1e-8)
        : lr_(learning_rate), decay_(decay_rate), eps_(epsilon) {
        if (lr_ <= 0.0 || !std::isfinite(lr_)) {
            throw std::invalid_argument("RMSProp: learning_rate must be positive and finite.");
        }
        if (decay_ < 0.0 || decay_ >= 1.0 || !std::isfinite(decay_)) {
            throw std::invalid_argument("RMSProp: decay_rate must satisfy 0.0 <= decay_rate < 1.0.");
        }
        if (eps_ <= 0.0 || !std::isfinite(eps_)) {
            throw std::invalid_argument("RMSProp: epsilon must be strictly positive and finite.");
        }
    }

    void step(std::vector<double>& params, const std::vector<double>& grads) override {
        validate_inputs(params, grads);

        if (v_.size() != params.size()) {
            v_.assign(params.size(), 0.0);
        }

        for (size_t i = 0; i < params.size(); ++i) {
            v_[i] = decay_ * v_[i] + (1.0 - decay_) * (grads[i] * grads[i]);
            params[i] -= (lr_ / (std::sqrt(v_[i]) + eps_)) * grads[i];
        }
        ++step_count_;
    }

    void reset() override {
        v_.clear();
        step_count_ = 0;
    }

    const char* name() const noexcept override {
        return "RMSProp";
    }

    const std::vector<double>& get_moving_average() const noexcept { return v_; }

private:
    double lr_;
    double decay_;
    double eps_;
    std::vector<double> v_;
};

/**
 * @brief Adam (Adaptive Moment Estimation).
 *
 * Computes individual adaptive learning rates for different parameters from estimates
 * of first (mean) and second (uncentered variance) moments of the gradients, including
 * bias-correction terms for initial iterations:
 *   m_t = \beta_1 * m_{t-1} + (1 - \beta_1) * g_t
 *   v_t = \beta_2 * v_{t-1} + (1 - \beta_2) * g_t^2
 *   \hat{m}_t = m_t / (1 - \beta_1^t)
 *   \hat{v}_t = v_t / (1 - \beta_2^t)
 *   \theta_t = \theta_{t-1} - \frac{\eta}{\sqrt{\hat{v}_t} + \epsilon} * \hat{m}_t
 */
class Adam : public Optimizer {
public:
    explicit Adam(double learning_rate, double beta1 = 0.9, double beta2 = 0.999, double epsilon = 1e-8)
        : lr_(learning_rate), beta1_(beta1), beta2_(beta2), eps_(epsilon) {
        if (lr_ <= 0.0 || !std::isfinite(lr_)) {
            throw std::invalid_argument("Adam: learning_rate must be positive and finite.");
        }
        if (beta1_ < 0.0 || beta1_ >= 1.0 || !std::isfinite(beta1_)) {
            throw std::invalid_argument("Adam: beta1 must satisfy 0.0 <= beta1 < 1.0.");
        }
        if (beta2_ < 0.0 || beta2_ >= 1.0 || !std::isfinite(beta2_)) {
            throw std::invalid_argument("Adam: beta2 must satisfy 0.0 <= beta2 < 1.0.");
        }
        if (eps_ <= 0.0 || !std::isfinite(eps_)) {
            throw std::invalid_argument("Adam: epsilon must be strictly positive and finite.");
        }
    }

    void step(std::vector<double>& params, const std::vector<double>& grads) override {
        validate_inputs(params, grads);

        if (m_.size() != params.size()) {
            m_.assign(params.size(), 0.0);
            v_.assign(params.size(), 0.0);
        }

        ++step_count_;
        const double bias_correction1 = 1.0 - std::pow(beta1_, static_cast<double>(step_count_));
        const double bias_correction2 = 1.0 - std::pow(beta2_, static_cast<double>(step_count_));

        for (size_t i = 0; i < params.size(); ++i) {
            m_[i] = beta1_ * m_[i] + (1.0 - beta1_) * grads[i];
            v_[i] = beta2_ * v_[i] + (1.0 - beta2_) * (grads[i] * grads[i]);

            const double m_hat = m_[i] / bias_correction1;
            const double v_hat = v_[i] / bias_correction2;

            params[i] -= (lr_ / (std::sqrt(v_hat) + eps_)) * m_hat;
        }
    }

    void reset() override {
        m_.clear();
        v_.clear();
        step_count_ = 0;
    }

    const char* name() const noexcept override {
        return "Adam";
    }

    const std::vector<double>& get_first_moment() const noexcept { return m_; }
    const std::vector<double>& get_second_moment() const noexcept { return v_; }

private:
    double lr_;
    double beta1_;
    double beta2_;
    double eps_;
    std::vector<double> m_;
    std::vector<double> v_;
};

} // namespace opt

#endif // OPTIMIZERS_HPP
