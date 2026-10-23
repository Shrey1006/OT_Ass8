/**
 * @file maxcut.hpp
 * @brief Maximum Cut (Max-Cut) Problem on Random Graphs in C++17.
 *
 * Implements:
 *  1. Graph representation with weighted undirected edges.
 *  2. Random Erdos-Renyi G(n, p) graph generator with deterministic seed.
 *  3. Greedy heuristic for Max-Cut.
 *  4. Exhaustive search (brute-force) finding exact global maximum cut.
 *  5. QUBO & Ising formulation:
 *     - Mapping: x_i in {0, 1} indicates partition assignment.
 *     - Cut(x) = sum_{(i,j) in E} w_ij * (x_i + x_j - 2 * x_i * x_j).
 *     - Minimizing x^T * Q * x maximizes Cut(x).
 *     - Binary-to-Spin x_i = (1 + s_i) / 2 maps directly to Ising Hamiltonian.
 *  6. JSON export facility emitting complete problem definition for QAOA in Track C.
 *
 * Optimization Techniques | Lab Assignment 8 (Track B)
 */

#ifndef MAXCUT_HPP
#define MAXCUT_HPP

#include "qubo.hpp"
#include <vector>
#include <string>
#include <fstream>
#include <random>
#include <algorithm>
#include <stdexcept>
#include <chrono>
#include <iostream>
#include <iomanip>

namespace opt {

struct Edge {
    size_t u;
    size_t v;
    double weight = 1.0;
};

class MaxCutGraph {
public:
    explicit MaxCutGraph(size_t num_nodes = 0)
        : num_nodes_(num_nodes), adj_matrix_(num_nodes, std::vector<double>(num_nodes, 0.0)) {}

    size_t num_nodes() const noexcept { return num_nodes_; }
    size_t num_edges() const noexcept { return edges_.size(); }
    const std::vector<Edge>& edges() const noexcept { return edges_; }

    void add_edge(size_t u, size_t v, double weight = 1.0) {
        if (u >= num_nodes_ || v >= num_nodes_) {
            throw std::out_of_range("MaxCutGraph::add_edge: node index out of range.");
        }
        if (u == v) {
            throw std::invalid_argument("MaxCutGraph::add_edge: self-loops not permitted.");
        }
        if (weight <= 0.0) {
            throw std::invalid_argument("MaxCutGraph::add_edge: edge weight must be positive.");
        }

        if (u > v) std::swap(u, v);

        // Update if edge already exists, else append
        if (adj_matrix_[u][v] == 0.0) {
            edges_.push_back({u, v, weight});
        } else {
            for (auto& e : edges_) {
                if (e.u == u && e.v == v) {
                    e.weight = weight;
                    break;
                }
            }
        }
        adj_matrix_[u][v] = weight;
        adj_matrix_[v][u] = weight;
    }

    /**
     * @brief Generates an Erdos-Renyi random graph G(n, p) with fixed seed.
     */
    static MaxCutGraph generate_erdos_renyi(
        size_t n,
        double edge_prob = 0.5,
        double min_weight = 1.0,
        double max_weight = 5.0,
        unsigned int seed = 42) {
        if (n < 2) throw std::invalid_argument("generate_erdos_renyi: n must be >= 2.");
        if (edge_prob <= 0.0 || edge_prob > 1.0) {
            throw std::invalid_argument("generate_erdos_renyi: edge_prob must be in (0, 1].");
        }

        std::mt19937 gen(seed);
        std::uniform_real_distribution<double> prob_dist(0.0, 1.0);
        std::uniform_real_distribution<double> wt_dist(min_weight, max_weight);

        MaxCutGraph graph(n);
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = i + 1; j < n; ++j) {
                if (prob_dist(gen) <= edge_prob) {
                    double wt = std::round(wt_dist(gen));
                    graph.add_edge(i, j, wt);
                }
            }
        }

        // Guarantee graph has at least one edge
        if (graph.num_edges() == 0) {
            graph.add_edge(0, 1, 1.0);
        }

        return graph;
    }

    /**
     * @brief Evaluates the cut weight for a partition bitstring x in {0, 1}^n.
     */
    double compute_cut(const std::vector<int>& partition) const {
        if (partition.size() != num_nodes_) {
            throw std::invalid_argument("compute_cut: partition size mismatch.");
        }
        double cut = 0.0;
        for (const auto& e : edges_) {
            if (partition[e.u] != partition[e.v]) {
                cut += e.weight;
            }
        }
        return cut;
    }

    /**
     * @brief Greedy heuristic: iteratively moves vertices between sets to maximize gain.
     */
    std::pair<double, std::vector<int>> solve_greedy() const {
        std::vector<int> partition(num_nodes_, 0);

        // Simple deterministic greedy assignment based on maximum cut delta
        for (size_t i = 0; i < num_nodes_; ++i) {
            partition[i] = 0;
            double cut0 = compute_cut(partition);
            partition[i] = 1;
            double cut1 = compute_cut(partition);

            if (cut0 > cut1) {
                partition[i] = 0;
            }
        }

        // Local search 1-flip improvement
        bool improved = true;
        while (improved) {
            improved = false;
            for (size_t i = 0; i < num_nodes_; ++i) {
                double cur_cut = compute_cut(partition);
                partition[i] = 1 - partition[i];
                double new_cut = compute_cut(partition);
                if (new_cut > cur_cut) {
                    improved = true;
                } else {
                    partition[i] = 1 - partition[i]; // Revert
                }
            }
        }

        return {compute_cut(partition), partition};
    }

    /**
     * @brief Exhaustive search over all 2^{n-1} non-equivalent partitions (exact global optimum).
     */
    std::pair<double, std::vector<int>> solve_exhaustive() const {
        if (num_nodes_ > 24) {
            throw std::invalid_argument("solve_exhaustive: graph size too large for brute force (> 24).");
        }

        double max_cut = -1.0;
        std::vector<int> best_partition(num_nodes_, 0);

        // Fix x_0 = 0 to break bitwise symmetry
        const uint64_t total = 1ULL << (num_nodes_ - 1);
        for (uint64_t mask = 0; mask < total; ++mask) {
            std::vector<int> partition(num_nodes_, 0);
            for (size_t i = 1; i < num_nodes_; ++i) {
                partition[i] = (mask >> (i - 1)) & 1ULL;
            }
            double cut = compute_cut(partition);
            if (cut > max_cut) {
                max_cut = cut;
                best_partition = partition;
            }
        }

        return {max_cut, best_partition};
    }

    /**
     * @brief Formulates the Max-Cut problem as a QUBO matrix Q such that:
     *        x^T * Q * x = -Cut(x)
     *
     * Derivation:
     *   Cut(x) = sum_{(i,j) in E} w_ij * (x_i + x_j - 2 * x_i * x_j)
     *   -Cut(x) = sum_{(i,j) in E} w_ij * (2 * x_i * x_j - x_i - x_j)
     *
     *   Diagonal:     Q_ii = -sum_{j in N(i)} w_ij
     *   Off-diagonal: Q_ij = 2 * w_ij (for i < j)
     */
    QUBO to_qubo() const {
        QUBO qubo(num_nodes_);

        for (const auto& e : edges_) {
            // Linear negative terms on diagonal
            qubo.at(e.u, e.u) -= e.weight;
            qubo.at(e.v, e.v) -= e.weight;

            // Off-diagonal quadratic coupling
            qubo.at(e.u, e.v) += 2.0 * e.weight;
        }

        return qubo;
    }

    /**
     * @brief Formulates the Max-Cut problem directly into an Ising Hamiltonian:
     *        H_Ising(s) = (1/2) * sum_{(i,j) in E} w_ij * s_i * s_j - (1/2) * sum_{(i,j) in E} w_ij
     *
     * Notice: minimizing H_Ising(s) maximizes Cut(s).
     */
    IsingModel to_ising() const {
        return to_qubo().to_ising();
    }

    /**
     * @brief Exports the graph, QUBO matrix, and optimal solution to a JSON file for Track C.
     */
    void export_to_json(const std::string& filepath) const {
        auto opt_res = solve_exhaustive();
        double opt_cut = opt_res.first;
        const auto& opt_partition = opt_res.second;
        QUBO qubo = to_qubo();
        IsingModel ising = to_ising();

        std::ofstream out(filepath);
        if (!out.is_open()) {
            throw std::runtime_error("export_to_json: unable to open file for writing: " + filepath);
        }

        out << "{\n";
        out << "  \"num_nodes\": " << num_nodes_ << ",\n";
        out << "  \"num_edges\": " << edges_.size() << ",\n";
        out << "  \"optimal_cut\": " << opt_cut << ",\n";

        // Optimal bitstring
        out << "  \"optimal_partition\": [";
        for (size_t i = 0; i < opt_partition.size(); ++i) {
            out << opt_partition[i] << (i + 1 < opt_partition.size() ? ", " : "");
        }
        out << "],\n";

        // Edges
        out << "  \"edges\": [\n";
        for (size_t i = 0; i < edges_.size(); ++i) {
            out << "    {\"u\": " << edges_[i].u << ", \"v\": " << edges_[i].v
                << ", \"weight\": " << edges_[i].weight << "}"
                << (i + 1 < edges_.size() ? ",\n" : "\n");
        }
        out << "  ],\n";

        // QUBO Matrix
        out << "  \"qubo_matrix\": [\n";
        for (size_t i = 0; i < num_nodes_; ++i) {
            out << "    [";
            for (size_t j = 0; j < num_nodes_; ++j) {
                out << qubo.at(i, j) << (j + 1 < num_nodes_ ? ", " : "");
            }
            out << "]" << (i + 1 < num_nodes_ ? ",\n" : "\n");
        }
        out << "  ],\n";

        // Ising Couplings
        out << "  \"ising_offset\": " << ising.offset << ",\n";
        out << "  \"ising_couplings\": [\n";
        bool first = true;
        for (size_t i = 0; i < num_nodes_; ++i) {
            for (size_t j = i + 1; j < num_nodes_; ++j) {
                if (ising.J[i][j] != 0.0) {
                    if (!first) out << ",\n";
                    out << "    {\"u\": " << i << ", \"v\": " << j << ", \"J\": " << ising.J[i][j] << "}";
                    first = false;
                }
            }
        }
        out << "\n  ]\n";
        out << "}\n";

        out.close();
        std::cout << "[INFO] Exported Max-Cut QUBO JSON to: " << filepath << std::endl;
    }

private:
    size_t num_nodes_;
    std::vector<Edge> edges_;
    std::vector<std::vector<double>> adj_matrix_;
};

} // namespace opt

#endif // MAXCUT_HPP
