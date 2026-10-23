#ifndef PARALLEL_WORKLOAD_HPP
#define PARALLEL_WORKLOAD_HPP

#include <iostream>
#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <random>
#include <cmath>
#include <algorithm>
#include <functional>
#include <memory>
#include <queue>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace opt {

// ==============================================================================
// 1. Core Compute Task: Monte Carlo European Option Pricing
// ==============================================================================
struct MonteCarloTask {
    double S0 = 100.0;  // Initial stock price
    double K = 105.0;   // Strike price
    double r = 0.05;    // Risk-free interest rate
    double sigma = 0.2; // Volatility
    double T = 1.0;     // Maturity in years

    // Evaluates a batch of N paths deterministically given a seed
    double evaluate_batch(size_t num_paths, unsigned int seed) const {
        std::mt19937 rng(seed);
        std::normal_distribution<double> norm(0.0, 1.0);

        double drift = (r - 0.5 * sigma * sigma) * T;
        double vol_sqrt = sigma * std::sqrt(T);
        double discount = std::exp(-r * T);

        double payoff_sum = 0.0;
        for (size_t i = 0; i < num_paths; ++i) {
            double z = norm(rng);
            double ST = S0 * std::exp(drift + vol_sqrt * z);
            double payoff = std::max(0.0, ST - K);
            payoff_sum += payoff;
        }

        return discount * (payoff_sum / static_cast<double>(num_paths));
    }
};

// ==============================================================================
// 2. Work-Stealing Queue Implementation
// ==============================================================================
template <typename T>
class WorkStealingQueue {
private:
    std::deque<T> deque_;
    mutable std::mutex mutex_;

public:
    WorkStealingQueue() = default;

    // Push task to the back (local thread operation, LIFO)
    void push(T item) {
        std::lock_guard<std::mutex> lock(mutex_);
        deque_.push_back(std::move(item));
    }

    // Pop task from the back (local thread operation, LIFO for cache locality)
    bool pop(T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (deque_.empty()) return false;
        item = std::move(deque_.back());
        deque_.pop_back();
        return true;
    }

    // Steal task from the front (remote thread operation, FIFO)
    bool steal(T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (deque_.empty()) return false;
        item = std::move(deque_.front());
        deque_.pop_front();
        return true;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return deque_.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return deque_.size();
    }
};

// ==============================================================================
// 3. Work-Stealing Thread Pool
// ==============================================================================
class WorkStealingThreadPool {
private:
    size_t num_threads_;
    std::vector<std::unique_ptr<WorkStealingQueue<std::function<void()>>>> queues_;
    std::vector<std::thread> workers_;
    std::atomic<bool> done_{false};
    std::atomic<size_t> remaining_tasks_{0};

    void worker_loop(size_t my_id) {
        while (!done_) {
            std::function<void()> task;
            bool found = false;

            // 1. Try local queue first (LIFO)
            if (queues_[my_id]->pop(task)) {
                found = true;
            } else {
                // 2. Steal from other queues (FIFO)
                for (size_t offset = 1; offset < num_threads_; ++offset) {
                    size_t victim = (my_id + offset) % num_threads_;
                    if (queues_[victim]->steal(task)) {
                        found = true;
                        break;
                    }
                }
            }

            if (found) {
                task();
                remaining_tasks_.fetch_sub(1, std::memory_order_relaxed);
            } else {
                std::this_thread::yield();
            }
        }
    }

public:
    explicit WorkStealingThreadPool(size_t num_threads) : num_threads_(num_threads) {
        for (size_t i = 0; i < num_threads_; ++i) {
            queues_.push_back(std::make_unique<WorkStealingQueue<std::function<void()>>>());
        }
        for (size_t i = 0; i < num_threads_; ++i) {
            workers_.emplace_back(&WorkStealingThreadPool::worker_loop, this, i);
        }
    }

    ~WorkStealingThreadPool() {
        wait_all();
        done_ = true;
        for (auto& w : workers_) {
            if (w.joinable()) w.join();
        }
    }

    void submit(size_t target_thread, std::function<void()> task) {
        remaining_tasks_.fetch_add(1, std::memory_order_relaxed);
        queues_[target_thread % num_threads_]->push(std::move(task));
    }

    void wait_all() {
        while (remaining_tasks_.load(std::memory_order_relaxed) > 0) {
            std::this_thread::yield();
        }
    }
};

// ==============================================================================
// 4. Benchmarking Engine: OpenMP vs. std::thread (Static, Dynamic, Work-Stealing)
// ==============================================================================
struct BenchmarkResult {
    std::string framework;
    std::string schedule;
    size_t threads = 1;
    double runtime_ms = 0.0;
    double speedup = 1.0;
    double efficiency = 1.0;
    double computed_value = 0.0;
};

class ParallelBenchmark {
public:
    static double run_serial(size_t total_paths, size_t num_chunks, unsigned int base_seed) {
        MonteCarloTask mc;
        size_t paths_per_chunk = total_paths / num_chunks;
        double sum = 0.0;
        for (size_t i = 0; i < num_chunks; ++i) {
            sum += mc.evaluate_batch(paths_per_chunk, base_seed + static_cast<unsigned int>(i));
        }
        return sum / static_cast<double>(num_chunks);
    }

    static double run_openmp_static(size_t total_paths, size_t num_chunks, size_t threads, unsigned int base_seed) {
        MonteCarloTask mc;
        size_t paths_per_chunk = total_paths / num_chunks;
        double total_sum = 0.0;

#ifdef _OPENMP
        #pragma omp parallel for num_threads(threads) schedule(static) reduction(+:total_sum)
        for (int i = 0; i < static_cast<int>(num_chunks); ++i) {
            total_sum += mc.evaluate_batch(paths_per_chunk, base_seed + static_cast<unsigned int>(i));
        }
#else
        total_sum = run_serial(total_paths, num_chunks, base_seed) * num_chunks;
#endif
        return total_sum / static_cast<double>(num_chunks);
    }

    static double run_openmp_dynamic(size_t total_paths, size_t num_chunks, size_t threads, unsigned int base_seed) {
        MonteCarloTask mc;
        size_t paths_per_chunk = total_paths / num_chunks;
        double total_sum = 0.0;

#ifdef _OPENMP
        #pragma omp parallel for num_threads(threads) schedule(dynamic, 4) reduction(+:total_sum)
        for (int i = 0; i < static_cast<int>(num_chunks); ++i) {
            total_sum += mc.evaluate_batch(paths_per_chunk, base_seed + static_cast<unsigned int>(i));
        }
#else
        total_sum = run_serial(total_paths, num_chunks, base_seed) * num_chunks;
#endif
        return total_sum / static_cast<double>(num_chunks);
    }

    static double run_std_thread_static(size_t total_paths, size_t num_chunks, size_t num_threads, unsigned int base_seed) {
        MonteCarloTask mc;
        size_t paths_per_chunk = total_paths / num_chunks;
        std::vector<std::thread> workers;
        std::vector<double> partial_sums(num_threads, 0.0);

        auto worker = [&](size_t tid) {
            size_t start = tid * num_chunks / num_threads;
            size_t end = (tid + 1) * num_chunks / num_threads;
            double local_sum = 0.0;
            for (size_t i = start; i < end; ++i) {
                local_sum += mc.evaluate_batch(paths_per_chunk, base_seed + static_cast<unsigned int>(i));
            }
            partial_sums[tid] = local_sum;
        };

        for (size_t t = 0; t < num_threads; ++t) {
            workers.emplace_back(worker, t);
        }
        for (auto& w : workers) w.join();

        double total = 0.0;
        for (double v : partial_sums) total += v;
        return total / static_cast<double>(num_chunks);
    }

    static double run_std_thread_dynamic(size_t total_paths, size_t num_chunks, size_t num_threads, unsigned int base_seed) {
        MonteCarloTask mc;
        size_t paths_per_chunk = total_paths / num_chunks;
        std::atomic<size_t> chunk_counter{0};
        std::vector<std::thread> workers;
        std::vector<double> partial_sums(num_threads, 0.0);

        auto worker = [&](size_t tid) {
            double local_sum = 0.0;
            while (true) {
                size_t c = chunk_counter.fetch_add(1, std::memory_order_relaxed);
                if (c >= num_chunks) break;
                local_sum += mc.evaluate_batch(paths_per_chunk, base_seed + static_cast<unsigned int>(c));
            }
            partial_sums[tid] = local_sum;
        };

        for (size_t t = 0; t < num_threads; ++t) {
            workers.emplace_back(worker, t);
        }
        for (auto& w : workers) w.join();

        double total = 0.0;
        for (double v : partial_sums) total += v;
        return total / static_cast<double>(num_chunks);
    }

    static double run_std_thread_work_stealing(size_t total_paths, size_t num_chunks, size_t num_threads, unsigned int base_seed) {
        MonteCarloTask mc;
        size_t paths_per_chunk = total_paths / num_chunks;
        WorkStealingThreadPool pool(num_threads);

        std::vector<double> results(num_chunks, 0.0);
        for (size_t i = 0; i < num_chunks; ++i) {
            size_t target = i % num_threads;
            pool.submit(target, [&mc, &results, paths_per_chunk, base_seed, i]() {
                results[i] = mc.evaluate_batch(paths_per_chunk, base_seed + static_cast<unsigned int>(i));
            });
        }

        pool.wait_all();

        double total = 0.0;
        for (double v : results) total += v;
        return total / static_cast<double>(num_chunks);
    }
};

// ==============================================================================
// 5. Demonstrations of Data, Model, and Pipeline Parallelism
// ==============================================================================
class ParallelParadigmsDemo {
public:
    // A. DATA PARALLELISM: Partitioning a uniform array computation across threads
    static std::vector<double> run_data_parallelism(const std::vector<double>& input, size_t num_threads) {
        size_t n = input.size();
        std::vector<double> output(n, 0.0);
        std::vector<std::thread> workers;

        size_t chunk = (n + num_threads - 1) / num_threads;
        for (size_t t = 0; t < num_threads; ++t) {
            workers.emplace_back([&input, &output, t, chunk, n]() {
                size_t start = t * chunk;
                size_t end = std::min(start + chunk, n);
                for (size_t i = start; i < end; ++i) {
                    double x = input[i];
                    output[i] = std::sin(x) * std::cos(x) + std::sqrt(std::abs(x));
                }
            });
        }
        for (auto& w : workers) w.join();
        return output;
    }

    // B. MODEL PARALLELISM: Splitting layers of a 2-stage neural operator across distinct threads
    static std::vector<double> run_model_parallelism(const std::vector<double>& input) {
        // Stage 1 (Layer 1) weights held exclusively on Thread 1
        // Stage 2 (Layer 2) weights held exclusively on Thread 2
        size_t dim = input.size();
        std::vector<double> hidden(dim, 0.0);
        std::vector<double> output(dim, 0.0);

        std::mutex mtx;
        std::condition_variable cv;
        bool layer1_done = false;

        // Worker for Layer 1
        std::thread t1([&]() {
            for (size_t i = 0; i < dim; ++i) {
                hidden[i] = std::tanh(1.5 * input[i] + 0.1);
            }
            {
                std::lock_guard<std::mutex> lock(mtx);
                layer1_done = true;
            }
            cv.notify_one();
        });

        // Worker for Layer 2
        std::thread t2([&]() {
            {
                std::unique_lock<std::mutex> lock(mtx);
                cv.wait(lock, [&]() { return layer1_done; });
            }
            for (size_t i = 0; i < dim; ++i) {
                output[i] = 2.0 * hidden[i] - 0.5;
            }
        });

        t1.join();
        t2.join();
        return output;
    }

    // C. PIPELINE PARALLELISM: 3-Stage Streaming Concurrent Pipeline
    static size_t run_pipeline_parallelism(size_t num_items) {
        std::queue<int> q1_to_2;
        std::mutex mtx1;
        std::condition_variable cv1;
        bool stage1_done = false;

        std::queue<double> q2_to_3;
        std::mutex mtx2;
        std::condition_variable cv2;
        bool stage2_done = false;

        size_t stage3_count = 0;

        // Stage 1: Producer / Ingestion
        std::thread t1([&]() {
            for (size_t i = 0; i < num_items; ++i) {
                {
                    std::lock_guard<std::mutex> lock(mtx1);
                    q1_to_2.push(static_cast<int>(i));
                }
                cv1.notify_one();
            }
            {
                std::lock_guard<std::mutex> lock(mtx1);
                stage1_done = true;
            }
            cv1.notify_all();
        });

        // Stage 2: Transformer / Compute
        std::thread t2([&]() {
            while (true) {
                int val = 0;
                {
                    std::unique_lock<std::mutex> lock(mtx1);
                    cv1.wait(lock, [&]() { return !q1_to_2.empty() || stage1_done; });
                    if (q1_to_2.empty() && stage1_done) break;
                    val = q1_to_2.front();
                    q1_to_2.pop();
                }
                double res = std::sqrt(static_cast<double>(val) * 2.0 + 1.0);
                {
                    std::lock_guard<std::mutex> lock(mtx2);
                    q2_to_3.push(res);
                }
                cv2.notify_one();
            }
            {
                std::lock_guard<std::mutex> lock(mtx2);
                stage2_done = true;
            }
            cv2.notify_all();
        });

        // Stage 3: Consumer / Aggregator
        std::thread t3([&]() {
            while (true) {
                double val = 0.0;
                {
                    std::unique_lock<std::mutex> lock(mtx2);
                    cv2.wait(lock, [&]() { return !q2_to_3.empty() || stage2_done; });
                    if (q2_to_3.empty() && stage2_done) break;
                    val = q2_to_3.front();
                    q2_to_3.pop();
                }
                if (val >= 0.0) stage3_count++;
            }
        });

        t1.join();
        t2.join();
        t3.join();
        return stage3_count;
    }
};

} // namespace opt

#endif // PARALLEL_WORKLOAD_HPP
