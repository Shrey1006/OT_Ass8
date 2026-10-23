#include "toy_compiler.hpp"
#include "toy_optimizer.hpp"
#include "toy_regalloc.hpp"
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
    std::cout << " Track F3: Register Allocation as Graph Coloring (Greedy Kempe vs. Exact BnB DSATUR)                  \n";
    std::cout << "=======================================================================================================\n";

    auto benchmarks = get_all_benchmark_programs();
    toy::Optimizer optimizer;

    // We evaluate register allocation with target K = 3 physical registers to induce spilling pressure
    const int TARGET_K = 3;

    std::ofstream csv("F_compiler/results/register_allocation_benchmark.csv");
    if (csv.is_open()) {
        csv << "id,name,vars_count,greedy_spills,exact_spills,greedy_instrs,exact_instrs,greedy_bytes,exact_bytes,greedy_time_us,exact_time_us\n";
    }

    std::cout << "Target Physical Register Budget: K = " << TARGET_K << "\n\n";
    std::cout << std::left << std::setw(4) << "ID"
              << std::setw(20) << "Program Name"
              << std::setw(8) << "Vars"
              << std::setw(15) << "Greedy Spills"
              << std::setw(15) << "Exact Spills"
              << std::setw(15) << "Greedy Instrs"
              << std::setw(15) << "Exact Instrs"
              << std::setw(14) << "Greedy(us)"
              << std::setw(12) << "Exact(us)" << "\n";
    std::cout << "-------------------------------------------------------------------------------------------------------\n";

    int total_greedy_spills = 0;
    int total_exact_spills = 0;
    int total_greedy_instrs = 0;
    int total_exact_instrs = 0;
    double total_greedy_time = 0.0;
    double total_exact_time = 0.0;

    for (const auto& bench : benchmarks) {
        // Compile to optimized TAC
        toy::Lexer lexer(bench.source);
        toy::Parser parser(lexer.tokenize_all());
        auto unopt_tac = toy::TACGenerator().generate(parser.parse_program());
        auto opt_tac = optimizer.optimize(unopt_tac);

        // Build Interference Graph via Liveness Analysis
        auto graph = toy::build_interference_graph(opt_tac);

        // 1. Solve with Greedy Kempe
        auto alloc_greedy = toy::allocate_registers_greedy(graph, TARGET_K);
        auto reg_stats_greedy = toy::lower_to_register_machine(opt_tac, alloc_greedy);

        // 2. Solve with Exact BnB DSATUR
        auto alloc_exact = toy::allocate_registers_exact(graph, TARGET_K);
        auto reg_stats_exact = toy::lower_to_register_machine(opt_tac, alloc_exact);

        total_greedy_spills += alloc_greedy.spill_count;
        total_exact_spills += alloc_exact.spill_count;
        total_greedy_instrs += reg_stats_greedy.total_instructions;
        total_exact_instrs += reg_stats_exact.total_instructions;
        total_greedy_time += alloc_greedy.compile_time_us;
        total_exact_time += alloc_exact.compile_time_us;

        std::cout << std::left << std::setw(4) << bench.id
                  << std::setw(20) << bench.name
                  << std::setw(8) << graph.num_vertices()
                  << std::setw(15) << alloc_greedy.spill_count
                  << std::setw(15) << alloc_exact.spill_count
                  << std::setw(15) << reg_stats_greedy.total_instructions
                  << std::setw(15) << reg_stats_exact.total_instructions
                  << std::fixed << std::setprecision(1)
                  << std::setw(14) << alloc_greedy.compile_time_us
                  << std::setw(12) << alloc_exact.compile_time_us << "\n";

        if (csv.is_open()) {
            csv << bench.id << "," << bench.name << ","
                << graph.num_vertices() << ","
                << alloc_greedy.spill_count << "," << alloc_exact.spill_count << ","
                << reg_stats_greedy.total_instructions << "," << reg_stats_exact.total_instructions << ","
                << reg_stats_greedy.code_size_bytes << "," << reg_stats_exact.code_size_bytes << ","
                << alloc_greedy.compile_time_us << "," << alloc_exact.compile_time_us << "\n";
        }
    }

    std::cout << "=======================================================================================================\n";
    std::cout << " TOTAL SPILLS         : Greedy = " << total_greedy_spills << " | Exact = " << total_exact_spills << "\n";
    std::cout << " TOTAL INSTRUCTIONS   : Greedy = " << total_greedy_instrs << " | Exact = " << total_exact_instrs << "\n";
    std::cout << " TOTAL COMPILE TIME   : Greedy = " << total_greedy_time << " us | Exact = " << total_exact_time << " us\n";
    std::cout << "=======================================================================================================\n";

    return 0;
}
