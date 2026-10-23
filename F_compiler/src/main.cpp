#include "toy_compiler.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>

struct TestProgram {
    std::string name;
    std::string source;
    std::vector<int64_t> expected_output;
};

std::vector<TestProgram> get_15_test_programs() {
    return {
        {
            "p01_arithmetic",
            "x = 10 + 20 * 3 - 50 / 2;\n"
            "y = 100 % 7;\n"
            "print(x);\n"
            "print(y);\n",
            {45, 2}
        },
        {
            "p02_variable_assign",
            "a = 5;\n"
            "b = a * 4;\n"
            "c = a + b;\n"
            "a = c - 2;\n"
            "print(a);\n"
            "print(b);\n"
            "print(c);\n",
            {23, 20, 25}
        },
        {
            "p03_if_simple",
            "x = 42;\n"
            "flag = 0;\n"
            "if (x > 30) {\n"
            "    flag = 1;\n"
            "}\n"
            "print(flag);\n",
            {1}
        },
        {
            "p04_if_else",
            "val = 15;\n"
            "result = 0;\n"
            "if (val % 2 == 0) {\n"
            "    result = 100;\n"
            "} else {\n"
            "    result = 200;\n"
            "}\n"
            "print(result);\n",
            {200}
        },
        {
            "p05_nested_if",
            "score = 85;\n"
            "grade = 0;\n"
            "if (score >= 90) {\n"
            "    grade = 1;\n"
            "} else {\n"
            "    if (score >= 80) {\n"
            "        grade = 2;\n"
            "    } else {\n"
            "        grade = 3;\n"
            "    }\n"
            "}\n"
            "print(grade);\n",
            {2}
        },
        {
            "p06_while_countdown",
            "i = 5;\n"
            "sum = 0;\n"
            "while (i > 0) {\n"
            "    sum = sum + i;\n"
            "    i = i - 1;\n"
            "}\n"
            "print(sum);\n",
            {15}
        },
        {
            "p07_factorial",
            "n = 7;\n"
            "fact = 1;\n"
            "while (n > 1) {\n"
            "    fact = fact * n;\n"
            "    n = n - 1;\n"
            "}\n"
            "print(fact);\n",
            {5040}
        },
        {
            "p08_fibonacci",
            "n = 10;\n"
            "a = 0;\n"
            "b = 1;\n"
            "i = 0;\n"
            "while (i < n) {\n"
            "    temp = a + b;\n"
            "    a = b;\n"
            "    b = temp;\n"
            "    i = i + 1;\n"
            "}\n"
            "print(a);\n",
            {55}
        },
        {
            "p09_collatz",
            "n = 27;\n"
            "steps = 0;\n"
            "max_val = n;\n"
            "while (n != 1) {\n"
            "    if (n % 2 == 0) {\n"
            "        n = n / 2;\n"
            "    } else {\n"
            "        n = 3 * n + 1;\n"
            "    }\n"
            "    if (n > max_val) {\n"
            "        max_val = n;\n"
            "    }\n"
            "    steps = steps + 1;\n"
            "}\n"
            "print(steps);\n"
            "print(max_val);\n",
            {111, 9232}
        },
        {
            "p10_gcd_euclid",
            "a = 1071;\n"
            "b = 462;\n"
            "while (b != 0) {\n"
            "    rem = a % b;\n"
            "    a = b;\n"
            "    b = rem;\n"
            "}\n"
            "print(a);\n",
            {21}
        },
        {
            "p11_power_exp",
            "base = 3;\n"
            "exp = 7;\n"
            "res = 1;\n"
            "while (exp > 0) {\n"
            "    if (exp % 2 == 1) {\n"
            "        res = res * base;\n"
            "    }\n"
            "    base = base * base;\n"
            "    exp = exp / 2;\n"
            "}\n"
            "print(res);\n",
            {2187}
        },
        {
            "p12_prime_check",
            "n = 97;\n"
            "is_prime = 1;\n"
            "d = 2;\n"
            "while (d * d <= n) {\n"
            "    if (n % d == 0) {\n"
            "        is_prime = 0;\n"
            "    }\n"
            "    d = d + 1;\n"
            "}\n"
            "print(is_prime);\n"
            "n2 = 91;\n"
            "is_prime2 = 1;\n"
            "d2 = 2;\n"
            "while (d2 * d2 <= n2) {\n"
            "    if (n2 % d2 == 0) {\n"
            "        is_prime2 = 0;\n"
            "    }\n"
            "    d2 = d2 + 1;\n"
            "}\n"
            "print(is_prime2);\n",
            {1, 0}
        },
        {
            "p13_nested_loops",
            "total = 0;\n"
            "i = 1;\n"
            "while (i <= 5) {\n"
            "    j = 1;\n"
            "    while (j <= 5) {\n"
            "        total = total + i * j;\n"
            "        j = j + 1;\n"
            "    }\n"
            "    i = i + 1;\n"
            "}\n"
            "print(total);\n",
            {225}
        },
        {
            "p14_boolean_logic",
            "a = 10;\n"
            "b = 20;\n"
            "c = 30;\n"
            "r1 = 0;\n"
            "if (a < b && b < c) {\n"
            "    r1 = 1;\n"
            "}\n"
            "r2 = 0;\n"
            "if (a > b || c == 30) {\n"
            "    r2 = 1;\n"
            "}\n"
            "r3 = 0;\n"
            "if (!(a == b)) {\n"
            "    r3 = 1;\n"
            "}\n"
            "print(r1);\n"
            "print(r2);\n"
            "print(r3);\n",
            {1, 1, 1}
        },
        {
            "p15_sum_of_squares",
            "n = 10;\n"
            "sum = 0;\n"
            "k = 1;\n"
            "while (k <= n) {\n"
            "    sum = sum + k * k;\n"
            "    k = k + 1;\n"
            "}\n"
            "formula = n * (n + 1) * (2 * n + 1) / 6;\n"
            "diff = sum - formula;\n"
            "print(sum);\n"
            "print(diff);\n",
            {385, 0}
        }
    };
}

int main(int argc, char* argv[]) {
    std::cout << "========================================================================\n";
    std::cout << " Track F1: MiniLang Toy Compiler, TAC IR, Stack VM & 15 Test Programs  \n";
    std::cout << "========================================================================\n";

    auto test_cases = get_15_test_programs();

    std::ofstream csv("F_compiler/results/test_execution.csv");
    if (csv.is_open()) {
        csv << "test_id,name,tokens_count,tac_instructions,bytecode_instructions,vm_cycles,passed\n";
    }

    int passed_count = 0;

    std::cout << std::left << std::setw(6) << "ID"
              << std::setw(24) << "Program Name"
              << std::setw(10) << "Tokens"
              << std::setw(12) << "TAC Count"
              << std::setw(12) << "Bytecode"
              << std::setw(12) << "VM Cycles"
              << std::setw(10) << "Status" << "\n";
    std::cout << "------------------------------------------------------------------------\n";

    for (size_t i = 0; i < test_cases.size(); ++i) {
        const auto& test = test_cases[i];
        
        // Write source to file for user inspection
        std::string file_path = "F_compiler/tests/programs/" + test.name + ".toy";
        std::ofstream src_file(file_path);
        if (src_file.is_open()) {
            src_file << test.source;
        }

        auto artifacts = toy::compile_and_run(test.source);

        bool passed = (artifacts.execution.output == test.expected_output);
        if (passed) passed_count++;

        std::cout << std::left << std::setw(6) << (i + 1)
                  << std::setw(24) << test.name
                  << std::setw(10) << artifacts.tokens.size()
                  << std::setw(12) << artifacts.tac.size()
                  << std::setw(12) << artifacts.bytecode.code.size()
                  << std::setw(12) << artifacts.execution.cycles_executed
                  << std::setw(10) << (passed ? "[PASS]" : "[FAIL]") << "\n";

        if (csv.is_open()) {
            csv << (i + 1) << "," << test.name << ","
                << artifacts.tokens.size() << ","
                << artifacts.tac.size() << ","
                << artifacts.bytecode.code.size() << ","
                << artifacts.execution.cycles_executed << ","
                << (passed ? "true" : "false") << "\n";
        }
    }

    std::cout << "========================================================================\n";
    std::cout << " Test Results: " << passed_count << "/" << test_cases.size() << " Passed.\n\n";

    // Detailed showcase on Program 9 (Collatz) to show AST, TAC, Bytecode and Outputs
    std::cout << ">>> Showcase Sample: Program 9 (Collatz Conjecture Steps) <<<\n";
    const auto& collatz_test = test_cases[8];
    auto collatz_art = toy::compile_and_run(collatz_test.source);

    std::cout << "\n--- Generated Three-Address-Code (TAC) IR (First 20 instructions) ---\n";
    for (size_t j = 0; j < std::min<size_t>(20, collatz_art.tac.size()); ++j) {
        std::cout << "  " << std::setw(3) << j << ": " << collatz_art.tac[j].to_string() << "\n";
    }
    std::cout << "  ... (" << collatz_art.tac.size() << " total TAC instructions)\n";

    std::cout << "\n--- Stack VM Execution Output ---\n";
    for (size_t o = 0; o < collatz_art.execution.output.size(); ++o) {
        std::cout << "  Output[" << o << "] = " << collatz_art.execution.output[o] << "\n";
    }
    std::cout << "  Total VM Cycles: " << collatz_art.execution.cycles_executed << "\n";
    std::cout << "========================================================================\n";

    return (passed_count == static_cast<int>(test_cases.size())) ? 0 : 1;
}
