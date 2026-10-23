#include "toy_compiler.hpp"
#include "toy_optimizer.hpp"
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
    assert(tokens[3].type == toy::TokenType::TOK_LE);
    assert(tokens[4].type == toy::TokenType::TOK_INT_LITERAL && tokens[4].int_val == 100);
    assert(tokens[5].type == toy::TokenType::TOK_AND);
    assert(tokens[6].type == toy::TokenType::TOK_IDENT && tokens[6].text == "y");
    assert(tokens[7].type == toy::TokenType::TOK_NE);
    assert(tokens[8].type == toy::TokenType::TOK_INT_LITERAL && tokens[8].int_val == 0);
    assert(tokens[9].type == toy::TokenType::TOK_RPAREN);
    assert(tokens[10].type == toy::TokenType::TOK_LBRACE);
    assert(tokens[11].type == toy::TokenType::TOK_PRINT);
    assert(tokens[12].type == toy::TokenType::TOK_LPAREN);
    assert(tokens[13].type == toy::TokenType::TOK_INT_LITERAL && tokens[13].int_val == 42);
    assert(tokens[14].type == toy::TokenType::TOK_RPAREN);
    assert(tokens[15].type == toy::TokenType::TOK_SEMICOLON);
    assert(tokens[16].type == toy::TokenType::TOK_RBRACE);
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
    // x is dynamically set across branches so it is not a known constant
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
    // x and y dynamically initialized across branch
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
    assert(res.output[0] == 1600); // (15+25) * (15+25) = 40 * 40 = 1600
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

void test_optimization_equivalence_all_15_programs() {
    std::cout << "[Test 7] Equivalence Harness: Unoptimized vs. Optimized (All 15 Programs)...\n";
    struct TC {
        std::string name;
        std::string src;
        std::vector<int64_t> exp;
    };
    std::vector<TC> cases = {
        {"P01: Arithmetic", "x = 10 + 20 * 3 - 50 / 2; y = 100 % 7; print(x); print(y);", {45, 2}},
        {"P02: Variable Assign", "a = 5; b = a * 4; c = a + b; a = c - 2; print(a); print(b); print(c);", {23, 20, 25}},
        {"P03: If Simple", "x = 42; flag = 0; if (x > 30) { flag = 1; } print(flag);", {1}},
        {"P04: If-Else", "val = 15; res = 0; if (val % 2 == 0) { res = 100; } else { res = 200; } print(res);", {200}},
        {"P05: Nested If", "score = 85; g = 0; if (score >= 90) { g = 1; } else { if (score >= 80) { g = 2; } else { g = 3; } } print(g);", {2}},
        {"P06: While Countdown", "i = 5; sum = 0; while (i > 0) { sum = sum + i; i = i - 1; } print(sum);", {15}},
        {"P07: Factorial", "n = 7; f = 1; while (n > 1) { f = f * n; n = n - 1; } print(f);", {5040}},
        {"P08: Fibonacci", "n = 10; a = 0; b = 1; i = 0; while (i < n) { t = a + b; a = b; b = t; i = i + 1; } print(a);", {55}},
        {"P09: Collatz", "n = 27; s = 0; m = n; while (n != 1) { if (n % 2 == 0) { n = n / 2; } else { n = 3 * n + 1; } if (n > m) { m = n; } s = s + 1; } print(s); print(m);", {111, 9232}},
        {"P10: Euclid GCD", "a = 1071; b = 462; while (b != 0) { r = a % b; a = b; b = r; } print(a);", {21}},
        {"P11: Fast Power", "b = 3; e = 7; res = 1; while (e > 0) { if (e % 2 == 1) { res = res * b; } b = b * b; e = e / 2; } print(res);", {2187}},
        {"P12: Prime Check", "n = 97; p = 1; d = 2; while (d * d <= n) { if (n % d == 0) { p = 0; } d = d + 1; } print(p); n2 = 91; p2 = 1; d2 = 2; while (d2 * d2 <= n2) { if (n2 % d2 == 0) { p2 = 0; } d2 = d2 + 1; } print(p2);", {1, 0}},
        {"P13: Nested Loops", "tot = 0; i = 1; while (i <= 5) { j = 1; while (j <= 5) { tot = tot + i * j; j = j + 1; } i = i + 1; } print(tot);", {225}},
        {"P14: Boolean Logic", "a = 10; b = 20; c = 30; r1 = 0; if (a < b && b < c) { r1 = 1; } r2 = 0; if (a > b || c == 30) { r2 = 1; } r3 = 0; if (!(a == b)) { r3 = 1; } print(r1); print(r2); print(r3);", {1, 1, 1}},
        {"P15: Sum of Squares", "n = 10; sum = 0; k = 1; while (k <= n) { sum = sum + k * k; k = k + 1; } form = n * (n + 1) * (2 * n + 1) / 6; diff = sum - form; print(sum); print(diff);", {385, 0}}
    };

    toy::Optimizer opt;
    toy::BytecodeEmitter emitter;

    for (const auto& tc : cases) {
        // Unoptimized
        auto unopt_art = toy::compile_and_run(tc.src);
        assert(unopt_art.execution.output == tc.exp);

        // Optimized
        auto opt_tac = opt.optimize(unopt_art.tac);
        auto opt_bc = emitter.compile(opt_tac);
        toy::StackVM vm;
        auto opt_res = vm.execute(opt_bc);

        // Assert exact semantic equivalence
        assert(opt_res.output == tc.exp);
        assert(opt_res.output == unopt_art.execution.output);
        assert(opt_tac.size() <= unopt_art.tac.size());

        std::cout << "  - [" << tc.name << "] EQUIVALENT (TAC: " 
                  << unopt_art.tac.size() << " -> " << opt_tac.size() << ")\n";
    }
}

int main() {
    std::cout << "=========================================================\n";
    std::cout << " Running Track F2: Optimization Passes Unit Tests        \n";
    std::cout << "=========================================================\n";

    test_lexer();
    test_parser_precedence();
    test_pass_constant_folding();
    test_pass_strength_reduction();
    test_pass_cse();
    test_pass_dce();
    test_optimization_equivalence_all_15_programs();

    std::cout << "\n>>> All Optimization Passes & Equivalence Tests PASSED! <<<\n";
    return 0;
}
