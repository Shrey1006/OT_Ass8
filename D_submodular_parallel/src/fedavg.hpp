#ifndef FEDAVG_HPP
#define FEDAVG_HPP

#include <vector>
#include <string>
#include <cmath>
#include <random>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <chrono>
#include <cassert>
#include <stdexcept>

namespace opt {

struct LogisticModel {
    std::vector<double> weights;
    double bias = 0.0;

    explicit LogisticModel(size_t dim = 0) : weights(dim, 0.0), bias(0.0) {}

    static double sigmoid(double z) {
        if (z >= 40.0) return 1.0;
        if (z <= -40.0) return 0.0;
        return 1.0 / (1.0 + std::exp(-z));
    }

    double predict_proba(const std::vector<double>& x) const {
        double z = bias;
        for (size_t j = 0; j < weights.size(); ++j) {
            z += weights[j] * x[j];
        }
        return sigmoid(z);
    }

    int predict(const std::vector<double>& x) const {
        return predict_proba(x) >= 0.5 ? 1 : 0;
    }
};

struct Dataset {
    std::vector<std::vector<double>> X;
    std::vector<int> y;

    size_t size() const { return y.size(); }
    size_t dim() const { return X.empty() ? 0 : X[0].size(); }

    static std::pair<Dataset, Dataset> generate_train_test(size_t n_train, size_t n_test, size_t dim, unsigned int seed = 42) {
        std::mt19937 rng(seed);
        std::normal_distribution<double> norm(0.0, 1.0);
        std::uniform_real_distribution<double> unif(0.0, 1.0);

        std::vector<double> true_w(dim);
        for (size_t j = 0; j < dim; ++j) true_w[j] = 1.5 * norm(rng);
        double true_b = 0.2;

        auto make_ds = [&](size_t n) {
            Dataset ds;
            ds.X.resize(n, std::vector<double>(dim));
            ds.y.resize(n);
            for (size_t i = 0; i < n; ++i) {
                double z = true_b;
                for (size_t j = 0; j < dim; ++j) {
                    ds.X[i][j] = norm(rng);
                    z += true_w[j] * ds.X[i][j];
                }
                double prob = LogisticModel::sigmoid(z);
                ds.y[i] = (unif(rng) < prob) ? 1 : 0;
            }
            return ds;
        };

        return {make_ds(n_train), make_ds(n_test)};
    }
};

struct EvaluationMetrics {
    double loss = 0.0;
    double accuracy_percent = 0.0;
};

class LogisticLoss {
public:
    static EvaluationMetrics evaluate(const LogisticModel& model, const Dataset& data, double lambda_reg = 0.001) {
        if (data.size() == 0) return {0.0, 0.0};

        double total_loss = 0.0;
        size_t correct = 0;
        const double eps = 1e-15;

        for (size_t i = 0; i < data.size(); ++i) {
            double p = model.predict_proba(data.X[i]);
            p = std::clamp(p, eps, 1.0 - eps);

            int yi = data.y[i];
            total_loss -= (yi * std::log(p) + (1 - yi) * std::log(1.0 - p));

            int pred = (p >= 0.5) ? 1 : 0;
            if (pred == yi) correct++;
        }

        // L2 Regularization on weights
        double reg = 0.0;
        for (double w : model.weights) reg += w * w;
        total_loss = (total_loss / static_cast<double>(data.size())) + 0.5 * lambda_reg * reg;

        double acc = (static_cast<double>(correct) / static_cast<double>(data.size())) * 100.0;
        return {total_loss, acc};
    }

    static void compute_gradients(const LogisticModel& model, const Dataset& data,
                                  std::vector<double>& grad_w, double& grad_b,
                                  double lambda_reg = 0.001) {
        const size_t n = data.size();
        const size_t d = model.weights.size();
        grad_w.assign(d, 0.0);
        grad_b = 0.0;

        for (size_t i = 0; i < n; ++i) {
            double p = model.predict_proba(data.X[i]);
            double diff = p - static_cast<double>(data.y[i]);

            for (size_t j = 0; j < d; ++j) {
                grad_w[j] += diff * data.X[i][j];
            }
            grad_b += diff;
        }

        for (size_t j = 0; j < d; ++j) {
            grad_w[j] = (grad_w[j] / static_cast<double>(n)) + lambda_reg * model.weights[j];
        }
        grad_b /= static_cast<double>(n);
    }
};

struct FederatedRunResult {
    std::string method;
    size_t local_steps = 0;
    size_t comm_rounds = 0;
    size_t total_gradient_steps = 0;
    double final_test_loss = 0.0;
    double final_test_accuracy = 0.0;
    double runtime_ms = 0.0;
    size_t communication_bytes = 0;
};

class FederatedLearningSystem {
public:
    /**
     * @brief Partitions a dataset across K clients.
     * Supports both IID (uniform distribution) and Non-IID (label skew).
     */
    static std::vector<Dataset> partition_data(const Dataset& data, size_t num_clients, bool non_iid = false, unsigned int seed = 42) {
        std::vector<Dataset> client_data(num_clients);
        const size_t n = data.size();

        if (!non_iid) {
            // IID: Uniform random partitioning
            std::vector<size_t> indices(n);
            std::iota(indices.begin(), indices.end(), 0);
            std::mt19937 rng(seed);
            std::shuffle(indices.begin(), indices.end(), rng);

            for (size_t i = 0; i < n; ++i) {
                size_t cid = i % num_clients;
                client_data[cid].X.push_back(data.X[indices[i]]);
                client_data[cid].y.push_back(data.y[indices[i]]);
            }
        } else {
            // Non-IID: Label skew partitioning (clients receive predominantly class 0 or class 1)
            std::vector<size_t> class0_idx, class1_idx;
            for (size_t i = 0; i < n; ++i) {
                if (data.y[i] == 0) class0_idx.push_back(i);
                else class1_idx.push_back(i);
            }

            size_t c0_per_client = class0_idx.size() / num_clients;
            size_t c1_per_client = class1_idx.size() / num_clients;

            for (size_t cid = 0; cid < num_clients; ++cid) {
                // Skew: even clients receive 80% class 0, odd clients receive 80% class 1
                size_t take0 = (cid % 2 == 0) ? static_cast<size_t>(c0_per_client * 1.6) : static_cast<size_t>(c0_per_client * 0.4);
                size_t take1 = (cid % 2 == 1) ? static_cast<size_t>(c1_per_client * 1.6) : static_cast<size_t>(c1_per_client * 0.4);

                take0 = std::min(take0, class0_idx.size());
                take1 = std::min(take1, class1_idx.size());

                for (size_t i = 0; i < take0 && !class0_idx.empty(); ++i) {
                    client_data[cid].X.push_back(data.X[class0_idx.back()]);
                    client_data[cid].y.push_back(data.y[class0_idx.back()]);
                    class0_idx.pop_back();
                }
                for (size_t i = 0; i < take1 && !class1_idx.empty(); ++i) {
                    client_data[cid].X.push_back(data.X[class1_idx.back()]);
                    client_data[cid].y.push_back(data.y[class1_idx.back()]);
                    class1_idx.pop_back();
                }
            }
        }

        return client_data;
    }

    /**
     * @brief Centralized Training Baseline:
     * Pools all client data into one central dataset and runs standard SGD.
     */
    static FederatedRunResult train_centralized(const Dataset& train_data, const Dataset& test_data,
                                               size_t total_steps, double lr = 0.1, double lambda_reg = 0.001) {
        auto t_start = std::chrono::steady_clock::now();
        const size_t dim = train_data.dim();
        LogisticModel model(dim);

        std::vector<double> gw(dim);
        double gb = 0.0;

        for (size_t step = 0; step < total_steps; ++step) {
            LogisticLoss::compute_gradients(model, train_data, gw, gb, lambda_reg);
            for (size_t j = 0; j < dim; ++j) {
                model.weights[j] -= lr * gw[j];
            }
            model.bias -= lr * gb;
        }

        auto t_end = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();

        auto eval = LogisticLoss::evaluate(model, test_data, lambda_reg);
        return {
            "Centralized SGD",
            total_steps, // Local steps = total
            1,           // 1 single aggregation at end
            total_steps,
            eval.loss,
            eval.accuracy_percent,
            ms,
            0 // 0 communication bytes during training
        };
    }

    /**
     * @brief Federated Averaging (FedAvg - McMahan et al., 2017):
     * K clients each perform E local SGD steps, then server computes weighted average.
     */
    static FederatedRunResult train_fedavg(const std::vector<Dataset>& clients_data, const Dataset& test_data,
                                          size_t comm_rounds, size_t local_steps,
                                          double lr = 0.1, double lambda_reg = 0.001) {
        auto t_start = std::chrono::steady_clock::now();

        const size_t num_clients = clients_data.size();
        const size_t dim = test_data.dim();
        LogisticModel global_model(dim);

        size_t total_samples = 0;
        for (const auto& c : clients_data) total_samples += c.size();

        size_t bytes_per_round = 2 * num_clients * (dim * sizeof(double) + sizeof(double)); // Downlink + Uplink
        size_t total_comm_bytes = 0;

        std::vector<double> gw(dim);
        double gb = 0.0;

        for (size_t round = 0; round < comm_rounds; ++round) {
            total_comm_bytes += bytes_per_round;

            std::vector<LogisticModel> local_models(num_clients, global_model);

            // Parallel/local client updates
            for (size_t c = 0; c < num_clients; ++c) {
                for (size_t s = 0; s < local_steps; ++s) {
                    LogisticLoss::compute_gradients(local_models[c], clients_data[c], gw, gb, lambda_reg);
                    for (size_t j = 0; j < dim; ++j) {
                        local_models[c].weights[j] -= lr * gw[j];
                    }
                    local_models[c].bias -= lr * gb;
                }
            }

            // Server Federated Averaging aggregation: w_global = sum (n_k / n) * w_k
            std::fill(global_model.weights.begin(), global_model.weights.end(), 0.0);
            global_model.bias = 0.0;

            for (size_t c = 0; c < num_clients; ++c) {
                double weight_factor = static_cast<double>(clients_data[c].size()) / static_cast<double>(total_samples);
                for (size_t j = 0; j < dim; ++j) {
                    global_model.weights[j] += weight_factor * local_models[c].weights[j];
                }
                global_model.bias += weight_factor * local_models[c].bias;
            }
        }

        auto t_end = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();

        auto eval = LogisticLoss::evaluate(global_model, test_data, lambda_reg);
        return {
            "FedAvg (E=" + std::to_string(local_steps) + ", R=" + std::to_string(comm_rounds) + ")",
            local_steps,
            comm_rounds,
            comm_rounds * local_steps,
            eval.loss,
            eval.accuracy_percent,
            ms,
            total_comm_bytes
        };
    }
};

} // namespace opt

#endif // FEDAVG_HPP
