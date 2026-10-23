#pragma once

#include "toy_compiler.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <algorithm>
#include <iostream>

namespace toy {

class Optimizer {
public:
    struct OptimizationStats {
        size_t original_tac_count = 0;
        size_t optimized_tac_count = 0;
        int iterations = 0;
        int constants_folded = 0;
        int strength_reductions = 0;
        int cse_eliminations = 0;
        int dead_instructions_removed = 0;
    };

    /**
     * Runs full iterative fixed-point optimization suite until convergence.
     */
    std::vector<TACInstruction> optimize(const std::vector<TACInstruction>& input_tac, OptimizationStats* stats = nullptr) {
        std::vector<TACInstruction> tac = input_tac;
        if (stats) {
            *stats = OptimizationStats();
            stats->original_tac_count = input_tac.size();
        }

        bool changed = true;
        int iter = 0;
        const int MAX_ITERATIONS = 50;

        while (changed && iter < MAX_ITERATIONS) {
            changed = false;
            iter++;

            changed |= pass_constant_folding_and_propagation(tac, stats);
            changed |= pass_strength_reduction(tac, stats);
            changed |= pass_copy_propagation(tac);
            changed |= pass_common_subexpression_elimination(tac, stats);
            changed |= pass_dead_code_elimination(tac, stats);
        }

        if (stats) {
            stats->optimized_tac_count = tac.size();
            stats->iterations = iter;
        }
        return tac;
    }

    /**
     * Pass 1: Constant Folding & Constant Propagation
     * Evaluates compile-time known operands for arithmetic, relational, and boolean logic.
     */
    bool pass_constant_folding_and_propagation(std::vector<TACInstruction>& tac, OptimizationStats* stats = nullptr) {
        bool changed = false;
        std::unordered_map<std::string, int64_t> const_table;

        for (size_t i = 0; i < tac.size(); ++i) {
            auto& instr = tac[i];

            // Reset table at jump targets / basic block boundaries
            if (instr.op == TACOp::LABEL) {
                const_table.clear();
                continue;
            }

            // Constant definition: dst = imm
            if (instr.op == TACOp::CONST) {
                const_table[instr.dst] = instr.imm;
                continue;
            }

            // Copy definition: dst = src1
            if (instr.op == TACOp::COPY) {
                if (const_table.count(instr.src1)) {
                    int64_t val = const_table[instr.src1];
                    instr.op = TACOp::CONST;
                    instr.imm = val;
                    instr.src1 = "";
                    const_table[instr.dst] = val;
                    changed = true;
                    if (stats) stats->constants_folded++;
                } else {
                    const_table.erase(instr.dst);
                }
                continue;
            }

            // Unary operators
            if (instr.op == TACOp::NEG || instr.op == TACOp::NOT) {
                if (const_table.count(instr.src1)) {
                    int64_t val = const_table[instr.src1];
                    int64_t res = (instr.op == TACOp::NEG) ? -val : (val == 0 ? 1 : 0);
                    instr.op = TACOp::CONST;
                    instr.imm = res;
                    instr.src1 = "";
                    const_table[instr.dst] = res;
                    changed = true;
                    if (stats) stats->constants_folded++;
                    continue;
                }
            }

            // Binary operators
            if (is_binary_op(instr.op)) {
                bool src1_const = const_table.count(instr.src1) > 0;
                bool src2_const = const_table.count(instr.src2) > 0;

                if (src1_const && src2_const) {
                    int64_t v1 = const_table[instr.src1];
                    int64_t v2 = const_table[instr.src2];

                    // Guard division/modulo by zero at compile time
                    if ((instr.op == TACOp::DIV || instr.op == TACOp::MOD) && v2 == 0) {
                        // Leave for runtime exception
                        continue;
                    }

                    int64_t folded_val = eval_binary_op(instr.op, v1, v2);
                    instr.op = TACOp::CONST;
                    instr.imm = folded_val;
                    instr.src1 = "";
                    instr.src2 = "";
                    const_table[instr.dst] = folded_val;
                    changed = true;
                    if (stats) stats->constants_folded++;
                    continue;
                }
            }

            // Invalidate destination if modified
            if (!instr.dst.empty()) {
                const_table.erase(instr.dst);
            }
        }
        return changed;
    }

    /**
     * Pass 2: Strength Reduction & Algebraic Identities
     * Replaces expensive operations (multiplication, division) with cheaper operations (addition, copy).
     */
    bool pass_strength_reduction(std::vector<TACInstruction>& tac, OptimizationStats* stats = nullptr) {
        bool changed = false;
        std::unordered_map<std::string, int64_t> const_table;

        for (size_t i = 0; i < tac.size(); ++i) {
            auto& instr = tac[i];
            if (instr.op == TACOp::LABEL) {
                const_table.clear();
                continue;
            }
            if (instr.op == TACOp::CONST) {
                const_table[instr.dst] = instr.imm;
                continue;
            }

            if (instr.op == TACOp::MUL) {
                bool s1_is_const = const_table.count(instr.src1) > 0;
                bool s2_is_const = const_table.count(instr.src2) > 0;
                int64_t c1 = s1_is_const ? const_table[instr.src1] : -999999;
                int64_t c2 = s2_is_const ? const_table[instr.src2] : -999999;

                // x * 0 or 0 * x -> 0
                if ((s2_is_const && c2 == 0) || (s1_is_const && c1 == 0)) {
                    instr.op = TACOp::CONST;
                    instr.imm = 0;
                    instr.src1 = "";
                    instr.src2 = "";
                    const_table[instr.dst] = 0;
                    changed = true;
                    if (stats) stats->strength_reductions++;
                    continue;
                }
                // x * 1 -> x
                if (s2_is_const && c2 == 1) {
                    instr.op = TACOp::COPY;
                    instr.src2 = "";
                    changed = true;
                    if (stats) stats->strength_reductions++;
                    continue;
                }
                // 1 * x -> x
                if (s1_is_const && c1 == 1) {
                    instr.op = TACOp::COPY;
                    instr.src1 = instr.src2;
                    instr.src2 = "";
                    changed = true;
                    if (stats) stats->strength_reductions++;
                    continue;
                }
                // x * 2 -> x + x
                if (s2_is_const && c2 == 2) {
                    instr.op = TACOp::ADD;
                    instr.src2 = instr.src1;
                    changed = true;
                    if (stats) stats->strength_reductions++;
                    continue;
                }
                // 2 * x -> x + x
                if (s1_is_const && c1 == 2) {
                    instr.op = TACOp::ADD;
                    instr.src1 = instr.src2;
                    changed = true;
                    if (stats) stats->strength_reductions++;
                    continue;
                }
            }

            if (instr.op == TACOp::ADD) {
                bool s1_is_const = const_table.count(instr.src1) > 0;
                bool s2_is_const = const_table.count(instr.src2) > 0;
                int64_t c1 = s1_is_const ? const_table[instr.src1] : -999999;
                int64_t c2 = s2_is_const ? const_table[instr.src2] : -999999;

                // x + 0 -> x
                if (s2_is_const && c2 == 0) {
                    instr.op = TACOp::COPY;
                    instr.src2 = "";
                    changed = true;
                    if (stats) stats->strength_reductions++;
                    continue;
                }
                // 0 + x -> x
                if (s1_is_const && c1 == 0) {
                    instr.op = TACOp::COPY;
                    instr.src1 = instr.src2;
                    instr.src2 = "";
                    changed = true;
                    if (stats) stats->strength_reductions++;
                    continue;
                }
            }

            if (instr.op == TACOp::SUB) {
                bool s2_is_const = const_table.count(instr.src2) > 0;
                int64_t c2 = s2_is_const ? const_table[instr.src2] : -999999;

                // x - 0 -> x
                if (s2_is_const && c2 == 0) {
                    instr.op = TACOp::COPY;
                    instr.src2 = "";
                    changed = true;
                    if (stats) stats->strength_reductions++;
                    continue;
                }
                // x - x -> 0
                if (instr.src1 == instr.src2) {
                    instr.op = TACOp::CONST;
                    instr.imm = 0;
                    instr.src1 = "";
                    instr.src2 = "";
                    const_table[instr.dst] = 0;
                    changed = true;
                    if (stats) stats->strength_reductions++;
                    continue;
                }
            }

            if (instr.op == TACOp::DIV) {
                bool s2_is_const = const_table.count(instr.src2) > 0;
                int64_t c2 = s2_is_const ? const_table[instr.src2] : -999999;

                // x / 1 -> x
                if (s2_is_const && c2 == 1) {
                    instr.op = TACOp::COPY;
                    instr.src2 = "";
                    changed = true;
                    if (stats) stats->strength_reductions++;
                    continue;
                }
            }

            if (!instr.dst.empty()) {
                const_table.erase(instr.dst);
            }
        }
        return changed;
    }

    /**
     * Pass 3: Copy Propagation
     * Propagates copy origins within basic blocks to unlock CSE and DCE.
     */
    bool pass_copy_propagation(std::vector<TACInstruction>& tac) {
        bool changed = false;
        std::unordered_map<std::string, std::string> copy_map;

        for (size_t i = 0; i < tac.size(); ++i) {
            auto& instr = tac[i];
            if (instr.op == TACOp::LABEL) {
                copy_map.clear();
                continue;
            }

            // Substitute src1 if in copy_map
            if (!instr.src1.empty() && copy_map.count(instr.src1)) {
                instr.src1 = copy_map[instr.src1];
                changed = true;
            }
            // Substitute src2 if in copy_map
            if (!instr.src2.empty() && copy_map.count(instr.src2)) {
                instr.src2 = copy_map[instr.src2];
                changed = true;
            }

            // Invalidate copies where dst is modified
            if (!instr.dst.empty()) {
                copy_map.erase(instr.dst);
                // Also remove any mappings pointing to dst
                for (auto it = copy_map.begin(); it != copy_map.end();) {
                    if (it->second == instr.dst) {
                        it = copy_map.erase(it);
                    } else {
                        ++it;
                    }
                }
            }

            // Record new copy: dst = src1 (only if src1 is not dst itself)
            if (instr.op == TACOp::COPY && instr.dst != instr.src1) {
                copy_map[instr.dst] = instr.src1;
            }
        }
        return changed;
    }

    /**
     * Pass 4: Common Sub-expression Elimination (CSE)
     * Replaces identical computations within basic blocks with existing results.
     */
    bool pass_common_subexpression_elimination(std::vector<TACInstruction>& tac, OptimizationStats* stats = nullptr) {
        bool changed = false;
        std::unordered_map<std::string, std::string> expr_cache;

        for (size_t i = 0; i < tac.size(); ++i) {
            auto& instr = tac[i];
            if (instr.op == TACOp::LABEL) {
                expr_cache.clear();
                continue;
            }

            // Invalidate prior expressions that depend on dst as an operand or prior value
            if (!instr.dst.empty()) {
                for (auto it = expr_cache.begin(); it != expr_cache.end();) {
                    if (it->second == instr.dst || it->first.find(instr.dst) != std::string::npos) {
                        it = expr_cache.erase(it);
                    } else {
                        ++it;
                    }
                }
            }

            if (is_binary_op(instr.op)) {
                std::string key = make_expr_key(instr.op, instr.src1, instr.src2);
                if (expr_cache.count(key)) {
                    std::string existing_var = expr_cache[key];
                    instr.op = TACOp::COPY;
                    instr.src1 = existing_var;
                    instr.src2 = "";
                    changed = true;
                    if (stats) stats->cse_eliminations++;
                } else {
                    expr_cache[key] = instr.dst;
                }
            }
        }
        return changed;
    }

    /**
     * Pass 5: Dead-Code Elimination (DCE)
     * Removes dead temporaries, no-op copies (x = x), and redundant jumps.
     */
    bool pass_dead_code_elimination(std::vector<TACInstruction>& tac, OptimizationStats* stats = nullptr) {
        bool changed = false;

        // Step 1: Count variable references across the entire program
        std::unordered_map<std::string, int> use_counts;
        for (const auto& instr : tac) {
            if (!instr.src1.empty()) use_counts[instr.src1]++;
            if (!instr.src2.empty()) use_counts[instr.src2]++;
        }

        // Step 2: Filter out dead instructions
        std::vector<TACInstruction> new_tac;
        new_tac.reserve(tac.size());

        for (size_t i = 0; i < tac.size(); ++i) {
            const auto& instr = tac[i];

            // Never remove labels, prints, or jumps
            if (instr.op == TACOp::LABEL || instr.op == TACOp::PRINT ||
                instr.op == TACOp::JUMP || instr.op == TACOp::JUMP_IF_ZERO) {
                // Check for jump to the immediately following label
                if (instr.op == TACOp::JUMP && i + 1 < tac.size()) {
                    if (tac[i + 1].op == TACOp::LABEL && tac[i + 1].dst == instr.dst) {
                        changed = true;
                        if (stats) stats->dead_instructions_removed++;
                        continue; // Skip redundant jump
                    }
                }
                new_tac.push_back(instr);
                continue;
            }

            // Remove no-op copy: x = x
            if (instr.op == TACOp::COPY && instr.dst == instr.src1) {
                changed = true;
                if (stats) stats->dead_instructions_removed++;
                continue;
            }

            // If destination is a compiler temporary (starts with 't') and has 0 uses, remove it
            if (!instr.dst.empty() && instr.dst[0] == 't' && use_counts[instr.dst] == 0) {
                changed = true;
                if (stats) stats->dead_instructions_removed++;
                continue;
            }

            new_tac.push_back(instr);
        }

        if (new_tac.size() != tac.size()) {
            tac = std::move(new_tac);
            changed = true;
        }
        return changed;
    }

private:
    static bool is_binary_op(TACOp op) {
        return (op == TACOp::ADD || op == TACOp::SUB || op == TACOp::MUL ||
                op == TACOp::DIV || op == TACOp::MOD || op == TACOp::EQ ||
                op == TACOp::NE || op == TACOp::LT || op == TACOp::LE ||
                op == TACOp::GT || op == TACOp::GE || op == TACOp::AND ||
                op == TACOp::OR);
    }

    static bool is_commutative(TACOp op) {
        return (op == TACOp::ADD || op == TACOp::MUL || op == TACOp::EQ ||
                op == TACOp::NE || op == TACOp::AND || op == TACOp::OR);
    }

    static std::string make_expr_key(TACOp op, const std::string& s1, const std::string& s2) {
        std::string a = s1;
        std::string b = s2;
        if (is_commutative(op) && a > b) {
            std::swap(a, b);
        }
        return tac_op_to_string(op) + " " + a + " " + b;
    }

    static int64_t eval_binary_op(TACOp op, int64_t v1, int64_t v2) {
        switch (op) {
            case TACOp::ADD: return v1 + v2;
            case TACOp::SUB: return v1 - v2;
            case TACOp::MUL: return v1 * v2;
            case TACOp::DIV: return (v2 != 0) ? (v1 / v2) : 0;
            case TACOp::MOD: return (v2 != 0) ? (v1 % v2) : 0;
            case TACOp::EQ:  return (v1 == v2) ? 1 : 0;
            case TACOp::NE:  return (v1 != v2) ? 1 : 0;
            case TACOp::LT:  return (v1 < v2) ? 1 : 0;
            case TACOp::LE:  return (v1 <= v2) ? 1 : 0;
            case TACOp::GT:  return (v1 > v2) ? 1 : 0;
            case TACOp::GE:  return (v1 >= v2) ? 1 : 0;
            case TACOp::AND: return (v1 != 0 && v2 != 0) ? 1 : 0;
            case TACOp::OR:  return (v1 != 0 || v2 != 0) ? 1 : 0;
            default: return 0;
        }
    }
};

} // namespace toy
