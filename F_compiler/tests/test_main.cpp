#include "toy_compiler.hpp"
#include "toy_optimizer.hpp"
#include "toy_regalloc.hpp"
#include <iostream>
#include <cassert>

void test_lexer() {
    std::cout << "[Test 1] Lexer Tokenization & Error Handling..." << std::flush;
    std::string src = "if (x_1 <= 100 && y != 0) { print(42); } // comment\n";
    toy::Lexer lexer(src);
    auto tokens = lexer.tokenize_all();

    assert(tokens.size() == 18);
    assert(tokens[0].type == toy::TokenType::TOK_IF);
    assert(tokens[1].type == toy::TokenType::TOK_LPAREN);
    assert(tokens[2].type == toy::TokenType::TOK_IDENT && tokens[2].text == "x_1");
    assert(tokens[17].type == toy::TokenType::TOK_EOF);

    bool caught = false;
    try {
        toy::Lexer bad("@invalid");
        bad.tokenize_all();
    } catch (const std::runtime_error&) {
        caught = true;
    }
    assert(caught);
    std::cout << " PASSED\n";
}

void test_parser_precedence() {
    std::cout << "[Test 2] Parser Operator Precedence..." << std::flush;
    std::string src = "res = 2 + 3 * 4; print(res);";
    auto art = toy::compile_and_run(src);
    assert(art.execution.output.size() == 1);
    assert(art.execution.output[0] == 14);

    std::string src2 = "res = (2 + 3) * 4; print(res);";
    auto art2 = toy::compile_and_run(src2);
    assert(art2.execution.output[0] == 20);

    bool caught = false;
    try {
        toy::compile_and_run("x = 5 + ;");
    } catch (const std::runtime_error&) {
        caught = true;
    }
    assert(caught);
    std::cout << " PASSED\n";
}

void test_pass_constant_folding() {
    std::cout << "[Test 3] Pass: Constant Folding & Propagation..." << std::flush;
    std::string src = "a = 10 + 20; b = a * 2; print(b);";
    auto art = toy::compile_and_run(src);

    toy::Optimizer opt;
    toy::Optimizer::OptimizationStats stats;
    auto opt_tac = opt.optimize(art.tac, &stats);

    assert(stats.constants_folded >= 2);
    toy::BytecodeEmitter emitter;
    auto opt_bc = emitter.compile(opt_tac);
    toy::StackVM vm;
    auto res = vm.execute(opt_bc);

    assert(res.output.size() == 1);
    assert(res.output[0] == 60);
    std::cout << " PASSED\n";
}

void test_pass_strength_reduction() {
    std::cout << "[Test 4] Pass: Strength Reduction & Algebraic Identities..." << std::flush;
    std::string src = "cond = 1; if (cond > 0) { x = 7; } else { x = 9; } "
                      "y = x * 2; z = x * 1; w = x * 0; zero = x - x; "
                      "print(y); print(z); print(w); print(zero);";
    auto art = toy::compile_and_run(src);

    toy::Optimizer opt;
    toy::Optimizer::OptimizationStats stats;
    auto opt_tac = opt.optimize(art.tac, &stats);

    assert(stats.strength_reductions >= 1);
    toy::BytecodeEmitter emitter;
    auto opt_bc = emitter.compile(opt_tac);
    toy::StackVM vm;
    auto res = vm.execute(opt_bc);

    assert(res.output.size() == 4);
    assert(res.output[0] == 14);
    assert(res.output[1] == 7);
    assert(res.output[2] == 0);
    assert(res.output[3] == 0);
    std::cout << " PASSED\n";
}

void test_pass_cse() {
    std::cout << "[Test 5] Pass: Common Sub-expression Elimination (CSE)..." << std::flush;
    std::string src = "cond = 1; if (cond > 0) { x = 15; y = 25; } else { x = 10; y = 20; } "
                      "a = x + y; b = x + y; c = a * b; print(c);";
    auto art = toy::compile_and_run(src);

    toy::Optimizer opt;
    toy::Optimizer::OptimizationStats stats;
    auto opt_tac = opt.optimize(art.tac, &stats);

    assert(stats.cse_eliminations >= 1);
    toy::BytecodeEmitter emitter;
    auto opt_bc = emitter.compile(opt_tac);
    toy::StackVM vm;
    auto res = vm.execute(opt_bc);

    assert(res.output.size() == 1);
    assert(res.output[0] == 1600);
    std::cout << " PASSED\n";
}

void test_pass_dce() {
    std::cout << "[Test 6] Pass: Dead-Code Elimination (DCE)..." << std::flush;
    std::string src = "x = 10; dead_var = 999 * 50; print(x);";
    auto art = toy::compile_and_run(src);

    toy::Optimizer opt;
    toy::Optimizer::OptimizationStats stats;
    auto opt_tac = opt.optimize(art.tac, &stats);

    assert(stats.dead_instructions_removed > 0);
    assert(opt_tac.size() < art.tac.size());

    toy::BytecodeEmitter emitter;
    auto opt_bc = emitter.compile(opt_tac);
    toy::StackVM vm;
    auto res = vm.execute(opt_bc);

    assert(res.output.size() == 1);
    assert(res.output[0] == 10);
    std::cout << " PASSED\n";
}

void test_interference_graph_and_regalloc() {
    std::cout << "[Test 7] Interference Graph & Register Allocation (Greedy vs Exact)..." << std::flush;
    // Program with overlapping live ranges
    std::string src = "a = 1; b = 2; c = 3; d = a + b; e = b + c; f = d * e; print(f);";
    auto art = toy::compile_and_run(src);

    auto graph = toy::build_interference_graph(art.tac);
    assert(graph.num_vertices() > 0);

    // K = 2 should cause spills, K = 10 should not
    auto greedy_k2 = toy::allocate_registers_greedy(graph, 2);
    assert(greedy_k2.is_valid);
    assert(greedy_k2.spill_count > 0);

    auto exact_k2 = toy::allocate_registers_exact(graph, 2);
    assert(exact_k2.is_valid);
    assert(exact_k2.spill_count <= greedy_k2.spill_count);

    auto greedy_k10 = toy::allocate_registers_greedy(graph, 10);
    assert(greedy_k10.is_valid);
    assert(greedy_k10.spill_count == 0);

    auto exact_k10 = toy::allocate_registers_exact(graph, 10);
    assert(exact_k10.is_valid);
    assert(exact_k10.spill_count == 0);
    std::cout << " PASSED\n";
}

int main() {
    std::cout << "=========================================================\n";
    std::cout << " Running Track F3: Compiler & Register Allocation Tests  \n";
    std::cout << "=========================================================\n";

    test_lexer();
    test_parser_precedence();
    test_pass_constant_folding();
    test_pass_strength_reduction();
    test_pass_cse();
    test_pass_dce();
    test_interference_graph_and_regalloc();

    std::cout << "\n>>> All Register Allocation & Optimization Tests PASSED! <<<\n";
    return 0;
}
