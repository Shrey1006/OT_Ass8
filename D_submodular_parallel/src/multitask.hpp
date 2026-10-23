#ifndef MULTITASK_HPP
#define MULTITASK_HPP

#include <vector>
#include <string>
#include <cmath>
#include <random>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <chrono>
#include <cassert>

namespace opt {

/**
 * @brief Two-Task Multi-Task Model with Shared Bottom Representation.
 *
 * Architecture:
 *   Shared Layer:  z = W_shared * x + b_shared   (dim_in -> dim_shared)
 *   Task 1 Head:   y1_pred = W_head1 * z + b1    (dim_shared -> 1, Regression MSE)
 *   Task 2 Head:   y2_pred = W_head2 * z + b2    (dim_shared -> 1, Conflicting Regression / Classification)
 */
struct MultiTaskModel {
    size_t in_dim = 4;
    size_t shared_dim = 4;

    // Shared trunk parameters
    std::vector<std::vector<double>> W_shared; // shared_dim x in_dim
    std::vector<double> b_shared;              // shared_dim

    // Task 1 Head (Regression)
    std::vector<double> W1; // shared_dim
    double b1 = 0.0;

    // Task 2 Head (Conflicting Task)
    std::vector<double> W2; // shared_dim
    double b2 = 0.0;

    explicit MultiTaskModel(size_t in_d = 4, size_t sh_d = 4, unsigned int seed = 42)
        : in_dim(in_d), shared_dim(sh_d),
          W_shared(sh_d, std::vector<double>(in_d, 0.0)),
          b_shared(sh_d, 0.0),
          W1(sh_d, 0.0), b1(0.0),
          W2(sh_d, 0.0), b2(0.0) {
        
        std::mt19937 rng(seed);
        std::normal_distribution<double> dist(0.0, 0.5);

        for (size_t i = 0; i < shared_dim; ++i) {
            for (size_t j = 0; j < in_dim; ++j) {
                W_shared[i][j] = dist(rng);
            }
            W1[i] = dist(rng);
            W2[i] = dist(rng);
        }
    }

    // Forward pass
    void forward(const std::vector<double>& x,
                 std::vector<double>& z,
                 double& y1_pred,
                 double& y2_pred) const {
        z.assign(shared_dim, 0.0);
        for (size_t i = 0; i < shared_dim; ++i) {
            double val = b_shared[i];
            for (size_t j = 0; j < in_dim; ++j) {
                val += W_shared[i][j] * x[j];
            }
            z[i] = std::tanh(val); // Non-linear activation
        }

        y1_pred = b1;
        y2_pred = b2;
        for (size_t i = 0; i < shared_dim; ++i) {
            y1_pred += W1[i] * z[i];
            y2_pred += W2[i] * z[i];
        }
    }

    size_t shared_param_count() const {
        return shared_dim * in_dim + shared_dim;
    }

    // Flatten shared gradients into a 1D vector
    std::vector<double> flatten_shared_grad(const std::vector<std::vector<double>>& dW,
                                           const std::vector<double>& db) const {
        std::vector<double> flat;
        flat.reserve(shared_param_count());
        for (size_t i = 0; i < shared_dim; ++i) {
            for (size_t j = 0; j < in_dim; ++j) {
                flat.push_back(dW[i][j]);
            }
        }
        for (size_t i = 0; i < shared_dim; ++i) {
            flat.push_back(db[i]);
        }
        return flat;
    }

    // Unflatten 1D shared gradient back into dW and db
    void unflatten_shared_grad(const std::vector<double>& flat,
                               std::vector<std::vector<double>>& dW,
                               std::vector<double>& db) const {
        size_t idx = 0;
        for (size_t i = 0; i < shared_dim; ++i) {
            for (size_t j = 0; j < in_dim; ++j) {
                dW[i][j] = flat[idx++];
            }
        }
        for (size_t i = 0; i < shared_dim; ++i) {
            db[i] = flat[idx++];
        }
    }
};

struct MultiTaskSample {
    std::vector<double> x;
    double y1 = 0.0;
    double y2 = 0.0;
};

class MultiTaskDataset {
public:
    static std::vector<MultiTaskSample> generate_conflicting_dataset(size_t n, size_t dim = 4, unsigned int seed = 42) {
        std::mt19937 rng(seed);
        std::normal_distribution<double> dist(0.0, 1.0);
        std::vector<MultiTaskSample> data(n);

        // Conflicting targets:
        // Task 1 correlates positively with x[0] and x[1]
        // Task 2 correlates negatively with x[0] and x[1], but positively with x[2]
        for (size_t i = 0; i < n; ++i) {
            data[i].x.resize(dim);
            for (size_t j = 0; j < dim; ++j) {
                data[i].x[j] = dist(rng);
            }
            data[i].y1 = 1.5 * data[i].x[0] + 1.2 * data[i].x[1] + 0.1 * dist(rng);
            data[i].y2 = -1.4 * data[i].x[0] - 1.1 * data[i].x[1] + 2.0 * data[i].x[2] + 0.1 * dist(rng);
        }
        return data;
    }
};

struct MultiTaskResult {
    std::string method;
    double final_loss1 = 0.0;
    double final_loss2 = 0.0;
    double total_loss = 0.0;
    double final_cosine_similarity = 0.0;
    double runtime_ms = 0.0;
    std::vector<double> loss1_history;
    std::vector<double> loss2_history;
};

class MultiTaskOptimizer {
public:
    /**
     * @brief Evaluates task losses (MSE) on the given dataset.
     */
    static std::pair<double, double> compute_losses(const MultiTaskModel& model,
                                                    const std::vector<MultiTaskSample>& data) {
        double l1 = 0.0, l2 = 0.0;
        std::vector<double> z;
        double p1 = 0.0, p2 = 0.0;

        for (const auto& s : data) {
            model.forward(s.x, z, p1, p2);
            l1 += (p1 - s.y1) * (p1 - s.y1);
            l2 += (p2 - s.y2) * (p2 - s.y2);
        }
        return {l1 / static_cast<double>(data.size()), l2 / static_cast<double>(data.size())};
    }

    /**
     * @brief Computes analytical gradients for Task 1 and Task 2 separately.
     */
    static void compute_task_gradients(const MultiTaskModel& model,
                                      const std::vector<MultiTaskSample>& data,
                                      // Task 1 Gradients
                                      std::vector<double>& g1_shared,
                                      std::vector<double>& dW1, double& db1,
                                      // Task 2 Gradients
                                      std::vector<double>& g2_shared,
                                      std::vector<double>& dW2, double& db2) {
        const size_t n = data.size();
        const size_t sh_dim = model.shared_dim;
        const size_t in_d = model.in_dim;

        std::vector<std::vector<double>> dW_sh1(sh_dim, std::vector<double>(in_d, 0.0));
        std::vector<double> db_sh1(sh_dim, 0.0);
        dW1.assign(sh_dim, 0.0);
        db1 = 0.0;

        std::vector<std::vector<double>> dW_sh2(sh_dim, std::vector<double>(in_d, 0.0));
        std::vector<double> db_sh2(sh_dim, 0.0);
        dW2.assign(sh_dim, 0.0);
        db2 = 0.0;

        std::vector<double> z;
        double p1 = 0.0, p2 = 0.0;

        for (const auto& s : data) {
            model.forward(s.x, z, p1, p2);
            double err1 = 2.0 * (p1 - s.y1) / static_cast<double>(n);
            double err2 = 2.0 * (p2 - s.y2) / static_cast<double>(n);

            // Task 1 Head Gradients
            db1 += err1;
            for (size_t i = 0; i < sh_dim; ++i) {
                dW1[i] += err1 * z[i];
            }

            // Task 2 Head Gradients
            db2 += err2;
            for (size_t i = 0; i < sh_dim; ++i) {
                dW2[i] += err2 * z[i];
            }

            // Backprop to shared layer for Task 1
            for (size_t i = 0; i < sh_dim; ++i) {
                double dz1 = err1 * model.W1[i] * (1.0 - z[i] * z[i]); // tanh derivative
                db_sh1[i] += dz1;
                for (size_t j = 0; j < in_d; ++j) {
                    dW_sh1[i][j] += dz1 * s.x[j];
                }
            }

            // Backprop to shared layer for Task 2
            for (size_t i = 0; i < sh_dim; ++i) {
                double dz2 = err2 * model.W2[i] * (1.0 - z[i] * z[i]);
                db_sh2[i] += dz2;
                for (size_t j = 0; j < in_d; ++j) {
                    dW_sh2[i][j] += dz2 * s.x[j];
                }
            }
        }

        g1_shared = model.flatten_shared_grad(dW_sh1, db_sh1);
        g2_shared = model.flatten_shared_grad(dW_sh2, db_sh2);
    }

    /**
     * @brief Closed-form 2-Task Multiple-Gradient Descent Algorithm (MGDA - Sener & Koltun 2018).
     * Solves min_{gamma in [0, 1]} || (1 - gamma) g1 + gamma g2 ||^2.
     */
    static std::pair<double, std::vector<double>> solve_mgda_2task(const std::vector<double>& g1,
                                                                   const std::vector<double>& g2) {
        assert(g1.size() == g2.size());
        const size_t p = g1.size();

        double g1_norm_sq = 0.0;
        double g2_norm_sq = 0.0;
        double g1_dot_g2 = 0.0;

        for (size_t i = 0; i < p; ++i) {
            g1_norm_sq += g1[i] * g1[i];
            g2_norm_sq += g2[i] * g2[i];
            g1_dot_g2 += g1[i] * g2[i];
        }

        double denom = g1_norm_sq + g2_norm_sq - 2.0 * g1_dot_g2;
        double gamma = 0.5;

        if (denom > 1e-14) {
            gamma = (g1_norm_sq - g1_dot_g2) / denom;
        }

        gamma = std::clamp(gamma, 0.0, 1.0);

        std::vector<double> g_mgda(p);
        for (size_t i = 0; i < p; ++i) {
            g_mgda[i] = (1.0 - gamma) * g1[i] + gamma * g2[i];
        }

        return {gamma, g_mgda};
    }

    /**
     * @brief Computes cosine similarity between two gradient vectors.
     */
    static double cosine_similarity(const std::vector<double>& g1, const std::vector<double>& g2) {
        double dot = 0.0, n1 = 0.0, n2 = 0.0;
        for (size_t i = 0; i < g1.size(); ++i) {
            dot += g1[i] * g2[i];
            n1 += g1[i] * g1[i];
            n2 += g2[i] * g2[i];
        }
        if (n1 < 1e-14 || n2 < 1e-14) return 0.0;
        return dot / (std::sqrt(n1) * std::sqrt(n2));
    }

    /**
     * @brief Trains MultiTaskModel using Equal Weighting (L_total = 0.5 L1 + 0.5 L2).
     */
    static MultiTaskResult train_equal_weighting(MultiTaskModel model,
                                                 const std::vector<MultiTaskSample>& data,
                                                 size_t epochs = 100, double lr = 0.05) {
        auto t_start = std::chrono::steady_clock::now();
        std::vector<double> l1_hist, l2_hist;
        double final_cos = 0.0;

        std::vector<double> g1_sh, g2_sh, dW1, dW2;
        double db1 = 0.0, db2 = 0.0;

        for (size_t ep = 0; ep < epochs; ++ep) {
            auto losses = compute_losses(model, data);
            l1_hist.push_back(losses.first);
            l2_hist.push_back(losses.second);

            compute_task_gradients(model, data, g1_sh, dW1, db1, g2_sh, dW2, db2);
            final_cos = cosine_similarity(g1_sh, g2_sh);

            // Equal weighting: g_shared = 0.5 * g1 + 0.5 * g2
            std::vector<double> g_sh(g1_sh.size());
            for (size_t i = 0; i < g_sh.size(); ++i) {
                g_sh[i] = 0.5 * (g1_sh[i] + g2_sh[i]);
            }

            // Update shared trunk
            std::vector<std::vector<double>> dW_sh(model.shared_dim, std::vector<double>(model.in_dim));
            std::vector<double> db_sh(model.shared_dim);
            model.unflatten_shared_grad(g_sh, dW_sh, db_sh);

            for (size_t i = 0; i < model.shared_dim; ++i) {
                for (size_t j = 0; j < model.in_dim; ++j) {
                    model.W_shared[i][j] -= lr * dW_sh[i][j];
                }
                model.b_shared[i] -= lr * db_sh[i];
                // Update task heads
                model.W1[i] -= lr * dW1[i];
                model.W2[i] -= lr * dW2[i];
            }
            model.b1 -= lr * db1;
            model.b2 -= lr * db2;
        }

        auto t_end = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();
        auto final_l = compute_losses(model, data);

        return {"Equal Weighting", final_l.first, final_l.second,
                final_l.first + final_l.second, final_cos, ms, l1_hist, l2_hist};
    }

    /**
     * @brief Trains MultiTaskModel using MGDA (Multiple-Gradient Descent Algorithm).
     */
    static MultiTaskResult train_mgda(MultiTaskModel model,
                                      const std::vector<MultiTaskSample>& data,
                                      size_t epochs = 100, double lr = 0.05) {
        auto t_start = std::chrono::steady_clock::now();
        std::vector<double> l1_hist, l2_hist;
        double final_cos = 0.0;

        std::vector<double> g1_sh, g2_sh, dW1, dW2;
        double db1 = 0.0, db2 = 0.0;

        for (size_t ep = 0; ep < epochs; ++ep) {
            auto losses = compute_losses(model, data);
            l1_hist.push_back(losses.first);
            l2_hist.push_back(losses.second);

            compute_task_gradients(model, data, g1_sh, dW1, db1, g2_sh, dW2, db2);
            final_cos = cosine_similarity(g1_sh, g2_sh);

            // MGDA Pareto-stationary descent direction
            auto mgda_res = solve_mgda_2task(g1_sh, g2_sh);
            const auto& g_sh = mgda_res.second;

            // Update shared trunk
            std::vector<std::vector<double>> dW_sh(model.shared_dim, std::vector<double>(model.in_dim));
            std::vector<double> db_sh(model.shared_dim);
            model.unflatten_shared_grad(g_sh, dW_sh, db_sh);

            for (size_t i = 0; i < model.shared_dim; ++i) {
                for (size_t j = 0; j < model.in_dim; ++j) {
                    model.W_shared[i][j] -= lr * dW_sh[i][j];
                }
                model.b_shared[i] -= lr * db_sh[i];
                // Update task heads with their own respective gradients
                model.W1[i] -= lr * dW1[i];
                model.W2[i] -= lr * dW2[i];
            }
            model.b1 -= lr * db1;
            model.b2 -= lr * db2;
        }

        auto t_end = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();
        auto final_l = compute_losses(model, data);

        return {"MGDA (Sener-Koltun)", final_l.first, final_l.second,
                final_l.first + final_l.second, final_cos, ms, l1_hist, l2_hist};
    }

    /**
     * @brief Trains MultiTaskModel using GradNorm (Gradient Normalization - Chen et al. 2018).
     */
    static MultiTaskResult train_gradnorm(MultiTaskModel model,
                                         const std::vector<MultiTaskSample>& data,
                                         size_t epochs = 100, double lr = 0.05,
                                         double alpha = 0.5, double lr_w = 0.025) {
        auto t_start = std::chrono::steady_clock::now();
        std::vector<double> l1_hist, l2_hist;
        double final_cos = 0.0;

        double w1 = 1.0, w2 = 1.0; // Dynamic loss weights

        auto init_losses = compute_losses(model, data);
        double l1_0 = std::max(init_losses.first, 1e-6);
        double l2_0 = std::max(init_losses.second, 1e-6);

        std::vector<double> g1_sh, g2_sh, dW1, dW2;
        double db1 = 0.0, db2 = 0.0;

        for (size_t ep = 0; ep < epochs; ++ep) {
            auto losses = compute_losses(model, data);
            l1_hist.push_back(losses.first);
            l2_hist.push_back(losses.second);

            compute_task_gradients(model, data, g1_sh, dW1, db1, g2_sh, dW2, db2);
            final_cos = cosine_similarity(g1_sh, g2_sh);

            // Compute gradient norms on shared representation
            double g1_norm = 0.0, g2_norm = 0.0;
            for (size_t i = 0; i < g1_sh.size(); ++i) {
                g1_norm += g1_sh[i] * g1_sh[i];
                g2_norm += g2_sh[i] * g2_sh[i];
            }
            g1_norm = std::sqrt(g1_norm);
            g2_norm = std::sqrt(g2_norm);

            // Weighted gradient norms: G_i = w_i * ||g_i||
            double G1 = w1 * g1_norm;
            double G2 = w2 * g2_norm;
            double G_bar = (G1 + G2) / 2.0;

            // Relative inverse training rate
            double l1_ratio = losses.first / l1_0;
            double l2_ratio = losses.second / l2_0;
            double l_ratio_bar = (l1_ratio + l2_ratio) / 2.0;

            double r1 = l1_ratio / std::max(l_ratio_bar, 1e-12);
            double r2 = l2_ratio / std::max(l_ratio_bar, 1e-12);

            // Target gradient norms
            double target1 = G_bar * std::pow(r1, alpha);
            double target2 = G_bar * std::pow(r2, alpha);

            // GradNorm weight gradient: dL_grad / dw_i = sign(G_i - target_i) * ||g_i||
            double d_w1 = ((G1 > target1) ? 1.0 : -1.0) * g1_norm;
            double d_w2 = ((G2 > target2) ? 1.0 : -1.0) * g2_norm;

            w1 -= lr_w * d_w1;
            w2 -= lr_w * d_w2;
            w1 = std::max(0.01, w1);
            w2 = std::max(0.01, w2);

            // Renormalize weights to sum to 2.0 (number of tasks)
            double w_sum = w1 + w2;
            w1 = 2.0 * (w1 / w_sum);
            w2 = 2.0 * (w2 / w_sum);

            // Combined shared gradient: g_shared = w1 * g1 + w2 * g2
            std::vector<double> g_sh(g1_sh.size());
            for (size_t i = 0; i < g_sh.size(); ++i) {
                g_sh[i] = w1 * g1_sh[i] + w2 * g2_sh[i];
            }

            // Update shared trunk
            std::vector<std::vector<double>> dW_sh(model.shared_dim, std::vector<double>(model.in_dim));
            std::vector<double> db_sh(model.shared_dim);
            model.unflatten_shared_grad(g_sh, dW_sh, db_sh);

            for (size_t i = 0; i < model.shared_dim; ++i) {
                for (size_t j = 0; j < model.in_dim; ++j) {
                    model.W_shared[i][j] -= lr * dW_sh[i][j];
                }
                model.b_shared[i] -= lr * db_sh[i];
                // Update task heads
                model.W1[i] -= lr * (w1 * dW1[i]);
                model.W2[i] -= lr * (w2 * dW2[i]);
            }
            model.b1 -= lr * (w1 * db1);
            model.b2 -= lr * (w2 * db2);
        }

        auto t_end = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();
        auto final_l = compute_losses(model, data);

        return {"GradNorm (Chen et al.)", final_l.first, final_l.second,
                final_l.first + final_l.second, final_cos, ms, l1_hist, l2_hist};
    }
};

} // namespace opt

#endif // MULTITASK_HPP
