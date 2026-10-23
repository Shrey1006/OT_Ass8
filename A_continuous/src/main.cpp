/**
 * @file main.cpp
 * @brief Benchmark suite running 8 first-order optimizers on 6 continuous landscapes.
 *
 * Capabilities:
 *  1. Learning Rate Grid Tuning: Searches over eta in {1e-4, 1e-3, 5e-3, 1e-2, 5e-2, 0.1, 0.25, 0.5}.
 *  2. Trajectory Recording: Logs step-by-step convergence f(theta_t) for each optimizer on each landscape.
 *  3. Noisy Gradient Stochastic Runs: Tests resilience under additive Gaussian noise over 10 distinct seeds.
 *     Reports empirical Mean +/- Std for final loss, iterations, and wall-clock runtime per Rule 2.
 *  4. Output CSVs:
 *     - results/benchmark_summary.csv
 *     - results/lr_sensitivity.csv
 *     - results/convergence_trajectories.csv
 *     - results/noisy_benchmark.csv
 *  5. Characterizes convex vs. ill-conditioned vs. non-convex vs. saddle point dynamics.
 *
 * Optimization Techniques | Lab Assignment 8 (Track A)
 */

#include "optimizers.hpp"
#include "objectives.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <cmath>
#include <random>
#include <iomanip>
#include <algorithm>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define MKDIR(d) _mkdir(d)
#else
#include <sys/stat.h>
#define MKDIR(d) mkdir(d, 0755)
#endif

namespace {

struct RunStats {
    double final_loss = 0.0;
    size_t iterations = 0;
    double runtime_us = 0.0;
    bool converged = false;
};

struct AggregateStats {
    double loss_mean = 0.0;
    double loss_std = 0.0;
    double iter_mean = 0.0;
    double iter_std = 0.0;
    double runtime_mean = 0.0;
    double runtime_std = 0.0;
};

AggregateStats compute_stats(const std::vector<RunStats>& runs) {
    AggregateStats agg;
    if (runs.empty()) return agg;

    const double N = static_cast<double>(runs.size());

    double sum_loss = 0.0, sum_iter = 0.0, sum_time = 0.0;
    for (const auto& r : runs) {
        sum_loss += r.final_loss;
        sum_iter += static_cast<double>(r.iterations);
        sum_time += r.runtime_us;
    }
    agg.loss_mean = sum_loss / N;
    agg.iter_mean = sum_iter / N;
    agg.runtime_mean = sum_time / N;

    if (runs.size() > 1) {
        double var_loss = 0.0, var_iter = 0.0, var_time = 0.0;
        for (const auto& r : runs) {
            var_loss += (r.final_loss - agg.loss_mean) * (r.final_loss - agg.loss_mean);
            var_iter += (static_cast<double>(r.iterations) - agg.iter_mean) * (static_cast<double>(r.iterations) - agg.iter_mean);
            var_time += (r.runtime_us - agg.runtime_mean) * (r.runtime_us - agg.runtime_mean);
        }
        agg.loss_std = std::sqrt(var_loss / (N - 1.0));
        agg.iter_std = std::sqrt(var_iter / (N - 1.0));
        agg.runtime_std = std::sqrt(var_time / (N - 1.0));
    }
    return agg;
}

std::unique_ptr<opt::Optimizer> create_optimizer(const std::string& name, double lr) {
    if (name == "GradientDescent") return std::make_unique<opt::GradientDescent>(lr);
    if (name == "SGD")             return std::make_unique<opt::SGD>(lr);
    if (name == "MiniBatchSGD")    return std::make_unique<opt::MiniBatchSGD>(lr, 4);
    if (name == "Momentum")        return std::make_unique<opt::Momentum>(lr, 0.9);
    if (name == "NAG")             return std::make_unique<opt::NAG>(lr, 0.9);
    if (name == "AdaGrad")         return std::make_unique<opt::AdaGrad>(lr);
    if (name == "RMSProp")         return std::make_unique<opt::RMSProp>(lr, 0.9);
    if (name == "Adam")            return std::make_unique<opt::Adam>(lr, 0.9, 0.999);
    throw std::invalid_argument("Unknown optimizer: " + name);
}

RunStats execute_run(
    opt::Optimizer& optimizer,
    const opt::ObjectiveFunction& objective,
    std::vector<double> x,
    size_t max_iters = 500,
    double grad_tol = 1e-5,
    double noise_sigma = 0.0,
    unsigned int noise_seed = 0,
    std::vector<double>* trajectory_out = nullptr) {

    optimizer.reset();
    std::mt19937 gen(noise_seed);
    std::normal_distribution<double> noise_dist(0.0, noise_sigma);

    RunStats stats;
    if (trajectory_out) {
        trajectory_out->clear();
        trajectory_out->push_back(objective.value(x));
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    for (size_t iter = 0; iter < max_iters; ++iter) {
        stats.iterations = iter + 1;
        auto grad = objective.gradient(x);

        // Check clean gradient stopping criteria
        double g_norm = 0.0;
        for (double g : grad) g_norm += g * g;
        g_norm = std::sqrt(g_norm);

        if (g_norm < grad_tol) {
            stats.converged = true;
            break;
        }

        // Add stochastic noise if enabled
        if (noise_sigma > 0.0) {
            for (double& g : grad) {
                g += noise_dist(gen);
            }
        }

        optimizer.step(x, grad);

        double current_loss = objective.value(x);
        if (trajectory_out) {
            trajectory_out->push_back(current_loss);
        }

        // Catch divergence early
        if (!std::isfinite(current_loss) || current_loss > 1e10) {
            stats.final_loss = 1e10;
            auto end_time = std::chrono::high_resolution_clock::now();
            stats.runtime_us = static_cast<double>(
                std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count());
            return stats;
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    stats.runtime_us = static_cast<double>(
        std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count());
    stats.final_loss = objective.value(x);

    return stats;
}

} // namespace

int main() {
    std::cout << "=====================================================================" << std::endl;
    std::cout << " Optimization Techniques: Track A Continuous Benchmarking Suite" << std::endl;
    std::cout << "=====================================================================" << std::endl;

    std::string results_dir = "results";
    MKDIR(results_dir.c_str());

    // Benchmark Objectives & Initial Conditions
    struct ObjConfig {
        std::unique_ptr<opt::ObjectiveFunction> obj;
        std::vector<double> x0;
        std::string landscape_type;
    };

    std::vector<ObjConfig> objectives;
    objectives.push_back({std::make_unique<opt::ConvexQuadratic>(), {3.0, 3.0}, "Convex"});
    objectives.push_back({std::make_unique<opt::StronglyConvexQuadratic>(), {3.0, 3.0}, "Strongly Convex (kappa=100)"});
    objectives.push_back({std::make_unique<opt::Rosenbrock>(1.0, 100.0), {-1.2, 1.0}, "Non-Convex Valley"});
    objectives.push_back({std::make_unique<opt::SaddlePoint>(), {0.5, 0.05}, "Indefinite Saddle"});
    objectives.push_back({std::make_unique<opt::Rastrigin>(), {2.5, 2.5}, "Multimodal Local Minima"});
    objectives.push_back({std::make_unique<opt::Himmelblau>(), {-1.0, -1.0}, "Multimodal 4-Minima"});

    const std::vector<std::string> optimizer_names = {
        "GradientDescent",
        "SGD",
        "MiniBatchSGD",
        "Momentum",
        "NAG",
        "AdaGrad",
        "RMSProp",
        "Adam"
    };

    const std::vector<double> lr_grid = {0.0001, 0.001, 0.005, 0.01, 0.05, 0.1, 0.25, 0.5};

    // CSV File Streams
    std::ofstream summary_csv(results_dir + "/benchmark_summary.csv");
    summary_csv << "Objective,Landscape_Type,Optimizer,Best_LR,Clean_Loss,Clean_Iters,Clean_Runtime_us,"
                << "Noisy_Loss_Mean,Noisy_Loss_Std,Noisy_Iters_Mean,Noisy_Iters_Std,Noisy_Time_Mean,Noisy_Time_Std\n";

    std::ofstream lr_csv(results_dir + "/lr_sensitivity.csv");
    lr_csv << "Objective,Optimizer,Learning_Rate,Final_Loss,Iterations,Runtime_us,Converged\n";

    std::ofstream traj_csv(results_dir + "/convergence_trajectories.csv");
    traj_csv << "Objective,Optimizer,Best_LR,Iteration,Loss\n";

    std::ofstream noisy_csv(results_dir + "/noisy_benchmark.csv");
    noisy_csv << "Objective,Optimizer,Seed,Final_Loss,Iterations,Runtime_us\n";

    std::cout << "[INFO] Tuning learning rates over grid and executing clean & noisy runs..." << std::endl;

    for (const auto& obj_entry : objectives) {
        const auto& obj = *(obj_entry.obj);
        const auto& x0 = obj_entry.x0;
        const auto& landscape = obj_entry.landscape_type;

        std::cout << "\n---------------------------------------------------------------------" << std::endl;
        std::cout << "Objective: " << obj.name() << " [" << landscape << "]" << std::endl;
        std::cout << "---------------------------------------------------------------------" << std::endl;

        for (const auto& opt_name : optimizer_names) {
            double best_lr = lr_grid[0];
            double best_loss = std::numeric_limits<double>::infinity();
            RunStats best_clean_run;

            // 1. Grid search for optimal step size
            for (double lr : lr_grid) {
                auto opt = create_optimizer(opt_name, lr);
                RunStats stats = execute_run(*opt, obj, x0, 500, 1e-5, 0.0, 0, nullptr);

                lr_csv << obj.name() << "," << opt_name << "," << lr << ","
                       << stats.final_loss << "," << stats.iterations << ","
                       << stats.runtime_us << "," << (stats.converged ? 1 : 0) << "\n";

                if (stats.final_loss < best_loss) {
                    best_loss = stats.final_loss;
                    best_lr = lr;
                    best_clean_run = stats;
                }
            }

            // 2. Record full trajectory for best learning rate
            auto best_opt = create_optimizer(opt_name, best_lr);
            std::vector<double> trajectory;
            execute_run(*best_opt, obj, x0, 500, 1e-5, 0.0, 0, &trajectory);

            for (size_t step = 0; step < trajectory.size(); ++step) {
                traj_csv << obj.name() << "," << opt_name << "," << best_lr << ","
                         << step << "," << trajectory[step] << "\n";
            }

            // 3. Stochastic noisy-gradient evaluation over 10 distinct fixed seeds (Rule 2)
            const size_t num_seeds = 10;
            const double noise_sigma = 0.05;
            std::vector<RunStats> noisy_runs;
            noisy_runs.reserve(num_seeds);

            for (size_t s = 0; s < num_seeds; ++s) {
                unsigned int seed = 101 + static_cast<unsigned int>(s);
                auto noisy_opt = create_optimizer(opt_name, best_lr);
                RunStats n_stats = execute_run(*noisy_opt, obj, x0, 500, 1e-5, noise_sigma, seed, nullptr);
                noisy_runs.push_back(n_stats);

                noisy_csv << obj.name() << "," << opt_name << "," << seed << ","
                          << n_stats.final_loss << "," << n_stats.iterations << ","
                          << n_stats.runtime_us << "\n";
            }

            AggregateStats noisy_agg = compute_stats(noisy_runs);

            // 4. Record to benchmark summary CSV
            summary_csv << obj.name() << "," << landscape << "," << opt_name << "," << best_lr << ","
                        << best_clean_run.final_loss << "," << best_clean_run.iterations << "," << best_clean_run.runtime_us << ","
                        << noisy_agg.loss_mean << "," << noisy_agg.loss_std << ","
                        << noisy_agg.iter_mean << "," << noisy_agg.iter_std << ","
                        << noisy_agg.runtime_mean << "," << noisy_agg.runtime_std << "\n";

            std::cout << "  " << std::setw(16) << std::left << opt_name
                      << " | Best LR: " << std::setw(6) << best_lr
                      << " | Clean Loss: " << std::setw(11) << best_clean_run.final_loss
                      << " | Noisy Loss (10 runs): " << std::fixed << std::setprecision(4)
                      << noisy_agg.loss_mean << " +/- " << noisy_agg.loss_std
                      << std::endl;
        }
    }

    summary_csv.close();
    lr_csv.close();
    traj_csv.close();
    noisy_csv.close();

    std::cout << "\n=====================================================================" << std::endl;
    std::cout << " Benchmark runs complete. Results written to 'results/' CSV files." << std::endl;
    std::cout << "=====================================================================" << std::endl;

    return 0;
}
