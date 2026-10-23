#pragma once

#include "toy_compiler.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <queue>
#include <limits>
#include <iomanip>

namespace toy {

/**
 * Interference Graph representation of program variables and temporaries.
 */
struct InterferenceGraph {
    std::vector<std::string> vars;
    std::unordered_map<std::string, int> var_to_id;
    std::vector<std::unordered_set<int>> adj;
    std::vector<int> spill_weights; // Estimated usage frequency for spilling priority

    int num_vertices() const {
        return static_cast<int>(vars.size());
    }

    int get_or_create_var(const std::string& name, int weight = 1) {
        auto it = var_to_id.find(name);
        if (it != var_to_id.end()) {
            spill_weights[it->second] += weight;
            return it->second;
        }
        int id = static_cast<int>(vars.size());
        vars.push_back(name);
        var_to_id[name] = id;
        adj.emplace_back();
        spill_weights.push_back(weight);
        return id;
    }

    void add_edge(int u, int v) {
        if (u == v || u < 0 || v < 0) return;
        adj[u].insert(v);
        adj[v].insert(u);
    }

    int degree(int u) const {
        return static_cast<int>(adj[u].size());
    }
};

/**
 * Result of register allocation.
 */
struct AllocationResult {
    std::string method_name;
    int num_registers_allowed = 0;
    int colors_used = 0;
    int spill_count = 0;
    std::unordered_map<std::string, int> var_to_reg; // 0..K-1 if allocated, -1 if spilled
    std::unordered_map<std::string, int> var_to_spill_slot; // 0..S-1 if spilled
    double compile_time_us = 0.0;
    bool is_valid = false;
};

/**
 * Liveness Analyzer: Performs backward dataflow analysis to construct the Interference Graph.
 */
inline InterferenceGraph build_interference_graph(const std::vector<TACInstruction>& tac) {
    InterferenceGraph graph;
    const size_t n = tac.size();
    if (n == 0) return graph;

    // Step 1: Collect all variable defs and uses per instruction
    std::vector<std::unordered_set<std::string>> def(n);
    std::vector<std::unordered_set<std::string>> use(n);
    std::unordered_map<std::string, size_t> label_to_pc;

    for (size_t i = 0; i < n; ++i) {
        const auto& instr = tac[i];
        if (instr.op == TACOp::LABEL) {
            label_to_pc[instr.dst] = i;
        }
        if (!instr.dst.empty() && instr.op != TACOp::LABEL && instr.op != TACOp::JUMP && instr.op != TACOp::JUMP_IF_ZERO) {
            def[i].insert(instr.dst);
            graph.get_or_create_var(instr.dst, 1);
        }
        if (!instr.src1.empty()) {
            use[i].insert(instr.src1);
            graph.get_or_create_var(instr.src1, 1);
        }
        if (!instr.src2.empty()) {
            use[i].insert(instr.src2);
            graph.get_or_create_var(instr.src2, 1);
        }
    }

    // Step 2: Control flow successors
    std::vector<std::vector<size_t>> succ(n);
    for (size_t i = 0; i < n; ++i) {
        const auto& instr = tac[i];
        if (instr.op == TACOp::JUMP) {
            if (label_to_pc.count(instr.dst)) {
                succ[i].push_back(label_to_pc[instr.dst]);
            }
        } else if (instr.op == TACOp::JUMP_IF_ZERO) {
            if (label_to_pc.count(instr.dst)) {
                succ[i].push_back(label_to_pc[instr.dst]);
            }
            if (i + 1 < n) succ[i].push_back(i + 1);
        } else {
            if (i + 1 < n) succ[i].push_back(i + 1);
        }
    }

    // Step 3: Fixed-point backward liveness iteration
    std::vector<std::unordered_set<std::string>> live_in(n);
    std::vector<std::unordered_set<std::string>> live_out(n);

    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
            // LiveOut[i] = Union of LiveIn[s] for s in succ[i]
            std::unordered_set<std::string> new_out;
            for (size_t s : succ[i]) {
                new_out.insert(live_in[s].begin(), live_in[s].end());
            }

            // LiveIn[i] = Use[i] union (LiveOut[i] \ Def[i])
            std::unordered_set<std::string> new_in = use[i];
            for (const auto& v : new_out) {
                if (def[i].find(v) == def[i].end()) {
                    new_in.insert(v);
                }
            }

            if (new_out.size() != live_out[i].size() || new_in.size() != live_in[i].size()) {
                changed = true;
                live_out[i] = std::move(new_out);
                live_in[i] = std::move(new_in);
            }
        }
    }

    // Step 4: Add interference edges between variables live simultaneously
    for (size_t i = 0; i < n; ++i) {
        for (const auto& d : def[i]) {
            int u = graph.var_to_id[d];
            for (const auto& l : live_out[i]) {
                if (d != l) {
                    int v = graph.var_to_id[l];
                    graph.add_edge(u, v);
                }
            }
        }
    }

    return graph;
}

// ============================================================================
// METHOD 1: Greedy Kempe / Chaitin-Briggs Simplification Heuristic
// ============================================================================

inline AllocationResult allocate_registers_greedy(const InterferenceGraph& graph, int K) {
    auto t0 = std::chrono::steady_clock::now();
    AllocationResult res;
    res.method_name = "Greedy-Kempe";
    res.num_registers_allowed = K;

    const int V = graph.num_vertices();
    if (V == 0) {
        res.is_valid = true;
        return res;
    }

    std::vector<int> deg(V);
    std::vector<bool> removed(V, false);
    for (int i = 0; i < V; ++i) deg[i] = graph.degree(i);

    std::vector<int> stack;
    int remaining = V;

    // Simplification & Spilling loop
    while (remaining > 0) {
        // Look for vertex with degree < K (simplifiable)
        int best_candidate = -1;
        for (int i = 0; i < V; ++i) {
            if (!removed[i] && deg[i] < K) {
                best_candidate = i;
                break;
            }
        }

        // If none, pick spill candidate (lowest weight / highest degree)
        if (best_candidate == -1) {
            double best_spill_metric = std::numeric_limits<double>::infinity();
            for (int i = 0; i < V; ++i) {
                if (!removed[i]) {
                    double metric = static_cast<double>(graph.spill_weights[i]) / std::max(1, deg[i]);
                    if (metric < best_spill_metric) {
                        best_spill_metric = metric;
                        best_candidate = i;
                    }
                }
            }
        }

        removed[best_candidate] = true;
        stack.push_back(best_candidate);
        remaining--;

        for (int neighbor : graph.adj[best_candidate]) {
            if (!removed[neighbor]) {
                deg[neighbor]--;
            }
        }
    }

    // Coloring phase: pop from stack and assign lowest available register < K
    std::vector<int> colors(V, -1);
    int current_spill_slot = 0;
    int max_color_used = 0;

    while (!stack.empty()) {
        int u = stack.back();
        stack.pop_back();

        std::vector<bool> neighbor_colors(K, false);
        for (int neighbor : graph.adj[u]) {
            if (colors[neighbor] >= 0 && colors[neighbor] < K) {
                neighbor_colors[colors[neighbor]] = true;
            }
        }

        int assigned_color = -1;
        for (int c = 0; c < K; ++c) {
            if (!neighbor_colors[c]) {
                assigned_color = c;
                break;
            }
        }

        if (assigned_color != -1) {
            colors[u] = assigned_color;
            max_color_used = std::max(max_color_used, assigned_color + 1);
            res.var_to_reg[graph.vars[u]] = assigned_color;
        } else {
            // Must spill to stack slot
            colors[u] = -1;
            res.var_to_reg[graph.vars[u]] = -1;
            res.var_to_spill_slot[graph.vars[u]] = current_spill_slot++;
            res.spill_count++;
        }
    }

    res.colors_used = max_color_used;

    // Validate no interference conflict
    res.is_valid = true;
    for (int u = 0; u < V; ++u) {
        if (colors[u] >= 0) {
            for (int v : graph.adj[u]) {
                if (colors[v] == colors[u]) {
                    res.is_valid = false;
                }
            }
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    res.compile_time_us = std::chrono::duration<double, std::micro>(t1 - t0).count();
    return res;
}

// ============================================================================
// METHOD 2: Exact Branch-and-Bound / DSATUR Graph Coloring & Spill Optimizer
// ============================================================================

inline AllocationResult allocate_registers_exact(const InterferenceGraph& graph, int K) {
    auto t0 = std::chrono::steady_clock::now();
    AllocationResult res;
    res.method_name = "Exact-BnB-DSATUR";
    res.num_registers_allowed = K;

    const int V = graph.num_vertices();
    if (V == 0) {
        res.is_valid = true;
        return res;
    }

    // First obtain upper bound using Greedy
    AllocationResult greedy_init = allocate_registers_greedy(graph, K);
    int best_spills = greedy_init.spill_count;
    std::vector<int> best_colors(V);
    for (int i = 0; i < V; ++i) {
        best_colors[i] = greedy_init.var_to_reg[graph.vars[i]];
    }

    // Branch-and-Bound search with DSATUR variable ordering
    std::vector<int> current_colors(V, -2); // -2 = unassigned, -1 = spilled, 0..K-1 = register
    std::vector<std::unordered_set<int>> sat_colors(V);

    auto get_next_dsatur_vertex = [&]() -> int {
        int best_v = -1;
        int max_sat = -1;
        int max_deg = -1;
        for (int i = 0; i < V; ++i) {
            if (current_colors[i] == -2) {
                int sat = static_cast<int>(sat_colors[i].size());
                int deg = graph.degree(i);
                if (sat > max_sat || (sat == max_sat && deg > max_deg)) {
                    max_sat = sat;
                    max_deg = deg;
                    best_v = i;
                }
            }
        }
        return best_v;
    };

    auto solve_bnb = [&](auto& self, int depth, int current_spills) -> void {
        // Prune if current spills already exceed or equal best spills found
        if (current_spills >= best_spills) {
            return;
        }

        int u = get_next_dsatur_vertex();
        if (u == -1) {
            // All vertices assigned with fewer spills
            best_spills = current_spills;
            best_colors = current_colors;
            return;
        }

        // Try registers 0..K-1
        std::vector<bool> used_by_neighbors(K, false);
        for (int v : graph.adj[u]) {
            if (current_colors[v] >= 0 && current_colors[v] < K) {
                used_by_neighbors[current_colors[v]] = true;
            }
        }

        // Try colors
        for (int c = 0; c < K; ++c) {
            if (!used_by_neighbors[c]) {
                current_colors[u] = c;
                // Update saturation of neighbors
                std::vector<int> updated_neighbors;
                for (int v : graph.adj[u]) {
                    if (current_colors[v] == -2 && sat_colors[v].insert(c).second) {
                        updated_neighbors.push_back(v);
                    }
                }

                self(self, depth + 1, current_spills);

                // Backtrack
                for (int v : updated_neighbors) {
                    sat_colors[v].erase(c);
                }
                current_colors[u] = -2;
            }
        }

        // Try spilling vertex u
        if (current_spills + 1 < best_spills) {
            current_colors[u] = -1; // Spilled
            self(self, depth + 1, current_spills + 1);
            current_colors[u] = -2;
        }
    };

    solve_bnb(solve_bnb, 0, 0);

    // Build final exact allocation result
    res.spill_count = best_spills;
    int current_spill_slot = 0;
    int max_color_used = 0;

    for (int i = 0; i < V; ++i) {
        int c = best_colors[i];
        if (c >= 0) {
            res.var_to_reg[graph.vars[i]] = c;
            max_color_used = std::max(max_color_used, c + 1);
        } else {
            res.var_to_reg[graph.vars[i]] = -1;
            res.var_to_spill_slot[graph.vars[i]] = current_spill_slot++;
        }
    }
    res.colors_used = max_color_used;

    // Validate validity
    res.is_valid = true;
    for (int u = 0; u < V; ++u) {
        int cu = best_colors[u];
        if (cu >= 0) {
            for (int v : graph.adj[u]) {
                if (best_colors[v] == cu) {
                    res.is_valid = false;
                }
            }
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    res.compile_time_us = std::chrono::duration<double, std::micro>(t1 - t0).count();
    return res;
}

// ============================================================================
// REGISTER CODE EMITTER & SPILL ACCOUNTING
// ============================================================================

struct RegisterMachineStats {
    int total_instructions = 0;
    int spill_loads = 0;
    int spill_stores = 0;
    size_t code_size_bytes = 0;
};

/**
 * Lowers TAC to 3-address register machine code, injecting spill load/stores
 * whenever a variable has been allocated to memory instead of a register.
 */
inline RegisterMachineStats lower_to_register_machine(
    const std::vector<TACInstruction>& tac,
    const AllocationResult& alloc
) {
    RegisterMachineStats stats;

    for (const auto& instr : tac) {
        if (instr.op == TACOp::LABEL) {
            continue;
        }

        // Spill loads for src1 and src2
        if (!instr.src1.empty()) {
            auto it = alloc.var_to_reg.find(instr.src1);
            if (it != alloc.var_to_reg.end() && it->second == -1) {
                stats.spill_loads++;
                stats.total_instructions++; // load [spill_slot] -> scratch_reg
            }
        }
        if (!instr.src2.empty()) {
            auto it = alloc.var_to_reg.find(instr.src2);
            if (it != alloc.var_to_reg.end() && it->second == -1) {
                stats.spill_loads++;
                stats.total_instructions++; // load [spill_slot] -> scratch_reg
            }
        }

        // Primary instruction execution
        stats.total_instructions++;

        // Spill store for dst
        if (!instr.dst.empty()) {
            auto it = alloc.var_to_reg.find(instr.dst);
            if (it != alloc.var_to_reg.end() && it->second == -1) {
                stats.spill_stores++;
                stats.total_instructions++; // store scratch_reg -> [spill_slot]
            }
        }
    }

    // Assuming standard 4-byte 3-address RISC encoding per instruction
    stats.code_size_bytes = stats.total_instructions * 4;
    return stats;
}

} // namespace toy
