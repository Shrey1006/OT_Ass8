#include "toy_compiler.hpp"
#include "toy_optimizer.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>

struct BenchmarkEntry {
    int id;
    std::string name;
    std::string source;
    std::vector<int64_t> expected_output;
};

std::vector<BenchmarkEntry> get_all_benchmark_programs() {
    return {
        {1, "p01_arithmetic", "x = 10 + 20 * 3 - 50 / 2;\ny = 100 % 7;\nprint(x);\nprint(y);\n", {45, 2}},
        {2, "p02_variable_assign", "a = 5;\nb = a * 4;\nc = a + b;\na = c - 2;\nprint(a);\nprint(b);\nprint(c);\n", {23, 20, 25}},
        {3, "p03_if_simple", "x = 42;\nflag = 0;\nif (x > 30) {\n    flag = 1;\n}\nprint(flag);\n", {1}},
        {4, "p04_if_else", "val = 15;\nresult = 0;\nif (val % 2 == 0) {\n    result = 100;\n} else {\n    result = 200;\n}\nprint(result);\n", {200}},
        {5, "p05_nested_if", "score = 85;\ngrade = 0;\nif (score >= 90) {\n    grade = 1;\n} else {\n    if (score >= 80) {\n        grade = 2;\n    } else {\n        grade = 3;\n    }\n}\nprint(grade);\n", {2}},
        {6, "p06_while_countdown", "i = 5;\nsum = 0;\nwhile (i > 0) {\n    sum = sum + i;\n    i = i - 1;\n}\nprint(sum);\n", {15}},
        {7, "p07_factorial", "n = 7;\nfact = 1;\nwhile (n > 1) {\n    fact = fact * n;\n    n = n - 1;\n}\nprint(fact);\n", {5040}},
        {8, "p08_fibonacci", "n = 10;\na = 0;\nb = 1;\ni = 0;\nwhile (i < n) {\n    temp = a + b;\n    a = b;\n    b = temp;\n    i = i + 1;\n}\nprint(a);\n", {55}},
        {9, "p09_collatz", "n = 27;\nsteps = 0;\nmax_val = n;\nwhile (n != 1) {\n    if (n % 2 == 0) {\n        n = n / 2;\n    } else {\n        n = 3 * n + 1;\n    }\n    if (n > max_val) {\n        max_val = n;\n    }\n    steps = steps + 1;\n}\nprint(steps);\nprint(max_val);\n", {111, 9232}},
        {10, "p10_gcd_euclid", "a = 1071;\nb = 462;\nwhile (b != 0) {\n    rem = a % b;\n    a = b;\n    b = rem;\n}\nprint(a);\n", {21}},
        {11, "p11_power_exp", "base = 3;\nexp = 7;\nres = 1;\nwhile (exp > 0) {\n    if (exp % 2 == 1) {\n        res = res * base;\n    }\n    base = base * base;\n    exp = exp / 2;\n}\nprint(res);\n", {2187}},
        {12, "p12_prime_check", "n = 97;\nis_prime = 1;\nd = 2;\nwhile (d * d <= n) {\n    if (n % d == 0) {\n        is_prime = 0;\n    }\n    d = d + 1;\n}\nprint(is_prime);\nn2 = 91;\nis_prime2 = 1;\nd2 = 2;\nwhile (d2 * d2 <= n2) {\n    if (n2 % d2 == 0) {\n        is_prime2 = 0;\n    }\n    d2 = d2 + 1;\n}\nprint(is_prime2);\n", {1, 0}},
        {13, "p13_nested_loops", "total = 0;\ni = 1;\nwhile (i <= 5) {\n    j = 1;\n    while (j <= 5) {\n        total = total + i * j;\n        j = j + 1;\n    }\n    i = i + 1;\n}\nprint(total);\n", {225}},
        {14, "p14_boolean_logic", "a = 10;\nb = 20;\nc = 30;\nr1 = 0;\nif (a < b && b < c) {\n    r1 = 1;\n}\nr2 = 0;\nif (a > b || c == 30) {\n    r2 = 1;\n}\nr3 = 0;\nif (!(a == b)) {\n    r3 = 1;\n}\nprint(r1);\nprint(r2);\nprint(r3);\n", {1, 1, 1}},
        {15, "p15_sum_of_squares", "n = 10;\nsum = 0;\nk = 1;\nwhile (k <= n) {\n    sum = sum + k * k;\n    k = k + 1;\n}\nformula = n * (n + 1) * (2 * n + 1) / 6;\ndiff = sum - formula;\nprint(sum);\nprint(diff);\n", {385, 0}}
    };
}

int main() {
    std::cout << "=======================================================================================================\n";
    std::cout << " Track F2: Compiler Optimization Passes (Folding, Propagation, DCE, CSE, Strength Reduction)          \n";
    std::cout << "=======================================================================================================\n";

    auto benchmarks = get_all_benchmark_programs();
    toy::Optimizer optimizer;

    std::ofstream csv("F_compiler/results/optimization_passes_benchmark.csv");
    if (csv.is_open()) {
        csv << "id,name,unopt_tac,opt_tac,tac_reduction_pct,unopt_cycles,opt_cycles,cycles_reduction_pct,folded,strength_red,cses,dces,output_match\n";
    }

    std::cout << std::left << std::setw(4) << "ID"
              << std::setw(22) << "Program Name"
              << std::setw(12) << "Unopt TAC"
              << std::setw(10) << "Opt TAC"
              << std::setw(12) << "TAC Red(%)"
              << std::setw(14) << "Unopt Cycles"
              << std::setw(12) << "Opt Cycles"
              << std::setw(14) << "Cycle Red(%)"
              << std::setw(10) << "Equiv" << "\n";
    std::cout << "-------------------------------------------------------------------------------------------------------\n";

    int total_unopt_tac = 0;
    int total_opt_tac = 0;
    int64_t total_unopt_cycles = 0;
    int64_t total_opt_cycles = 0;
    int all_passed = 0;

    for (const auto& bench : benchmarks) {
        // 1. Unoptimized Pipeline
        toy::Lexer lexer(bench.source);
        toy::Parser parser(lexer.tokenize_all());
        auto ast = parser.parse_program();

        toy::TACGenerator tac_gen;
        auto unopt_tac = tac_gen.generate(ast);

        toy::BytecodeEmitter emitter;
        auto unopt_bc = emitter.compile(unopt_tac);

        toy::StackVM vm_unopt;
        auto unopt_res = vm_unopt.execute(unopt_bc);

        // 2. Optimized Pipeline
        toy::Optimizer::OptimizationStats opt_stats;
        auto opt_tac = optimizer.optimize(unopt_tac, &opt_stats);

        auto opt_bc = emitter.compile(opt_tac);

        toy::StackVM vm_opt;
        auto opt_res = vm_opt.execute(opt_bc);

        // 3. Verification & Comparison
        bool match = (unopt_res.output == opt_res.output) && (opt_res.output == bench.expected_output);
        if (match) all_passed++;

        double tac_red_pct = 100.0 * (1.0 - static_cast<double>(opt_tac.size()) / unopt_tac.size());
        double cycle_red_pct = 100.0 * (1.0 - static_cast<double>(opt_res.cycles_executed) / unopt_res.cycles_executed);

        total_unopt_tac += unopt_tac.size();
        total_opt_tac += opt_tac.size();
        total_unopt_cycles += unopt_res.cycles_executed;
        total_opt_cycles += opt_res.cycles_executed;

        std::cout << std::left << std::setw(4) << bench.id
                  << std::setw(22) << bench.name
                  << std::setw(12) << unopt_tac.size()
                  << std::setw(10) << opt_tac.size()
                  << std::fixed << std::setprecision(1)
                  << std::setw(12) << tac_red_pct
                  << std::setw(14) << unopt_res.cycles_executed
                  << std::setw(12) << opt_res.cycles_executed
                  << std::setw(14) << cycle_red_pct
                  << std::setw(10) << (match ? "[PASS]" : "[FAIL]") << "\n";

        if (csv.is_open()) {
            csv << bench.id << "," << bench.name << ","
                << unopt_tac.size() << "," << opt_tac.size() << ","
                << tac_red_pct << ","
                << unopt_res.cycles_executed << "," << opt_res.cycles_executed << ","
                << cycle_red_pct << ","
                << opt_stats.constants_folded << ","
                << opt_stats.strength_reductions << ","
                << opt_stats.cse_eliminations << ","
                << opt_stats.dead_instructions_removed << ","
                << (match ? "true" : "false") << "\n";
        }
    }

    std::cout << "=======================================================================================================\n";
    double overall_tac_red = 100.0 * (1.0 - static_cast<double>(total_opt_tac) / total_unopt_tac);
    double overall_cycle_red = 100.0 * (1.0 - static_cast<double>(total_opt_cycles) / total_unopt_cycles);

    std::cout << " TOTAL TAC INSTRUCTIONS : " << total_unopt_tac << " -> " << total_opt_tac 
              << " (" << std::fixed << std::setprecision(1) << overall_tac_red << "% reduction)\n";
    std::cout << " TOTAL VM CYCLES        : " << total_unopt_cycles << " -> " << total_opt_cycles 
              << " (" << std::fixed << std::setprecision(1) << overall_cycle_red << "% reduction)\n";
    std::cout << " SEMANTIC EQUIVALENCE   : " << all_passed << "/" << benchmarks.size() << " Passed Ground-Truth Check.\n";
    std::cout << "=======================================================================================================\n";

    // Showcase Program 1 (Arithmetic Constant Folding)
    std::cout << "\n>>> Pass Deep-Dive: Program 1 (Constant Folding & DCE) <<<\n";
    toy::Lexer lexer_p1(benchmarks[0].source);
    toy::Parser parser_p1(lexer_p1.tokenize_all());
    auto unopt_tac_p1 = toy::TACGenerator().generate(parser_p1.parse_program());
    toy::Optimizer::OptimizationStats stats_p1;
    auto opt_tac_p1 = optimizer.optimize(unopt_tac_p1, &stats_p1);

    std::cout << "Unoptimized TAC (16 instructions):\n";
    for (size_t i = 0; i < unopt_tac_p1.size(); ++i) {
        std::cout << "  " << std::setw(2) << i << ": " << unopt_tac_p1[i].to_string() << "\n";
    }
    std::cout << "\nOptimized TAC (" << opt_tac_p1.size() << " instructions - folded at compile-time):\n";
    for (size_t i = 0; i < opt_tac_p1.size(); ++i) {
        std::cout << "  " << std::setw(2) << i << ": " << opt_tac_p1[i].to_string() << "\n";
    }
    std::cout << "=======================================================================================================\n";

    return (all_passed == static_cast<int>(benchmarks.size())) ? 0 : 1;
}
