#include <iostream>
#include <string>
#include <cstdlib>
#include <array>
#include <memory>
#include <fstream>
#include <sstream>
#include <vector>
#include <iomanip>

std::string exec_cmd(const char* cmd) {
    std::array<char, 256> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

void print_csv_table(const std::string& path, const std::string& title) {
    std::ifstream csv(path);
    if (!csv.is_open()) {
        std::string alt_path = "../" + path;
        csv.open(alt_path);
    }
    if (!csv.is_open()) {
        std::cout << "[WARNING] Could not open " << path << "\n";
        return;
    }

    std::cout << "---------------------------------------------------------------------------------------------------------\n";
    std::cout << " " << title << "\n";
    std::cout << "---------------------------------------------------------------------------------------------------------\n";

    std::string line;
    if (!std::getline(csv, line)) return; // Header

    // Print rows
    while (std::getline(csv, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> cols;

        while (ss.good()) {
            if (ss.peek() == '"') {
                ss.get();
                std::getline(ss, token, '"');
                if (ss.peek() == ',') ss.get();
            } else {
                std::getline(ss, token, ',');
            }
            cols.push_back(token);
        }

        if (cols.size() >= 5) {
            std::cout << std::left
                      << std::setw(38) << cols[0]
                      << std::setw(20) << cols[1]
                      << std::setw(16) << cols[2]
                      << std::setw(12) << cols[3]
                      << cols[4]
                      << "\n";
        }
    }
    std::cout << "---------------------------------------------------------------------------------------------------------\n";
}

int main() {
    std::cout << "========================================================================\n";
    std::cout << " Track I: Quantum Algorithms & QML vs Classical Benchmark Suite        \n";
    std::cout << "========================================================================\n\n";

    // 1. Run I1 Quantum Algorithms Suite
    std::cout << "[Step 1] Executing Track I1 Quantum Algorithms Suite (Qiskit 2.6 & D-Wave Neal)...\n";
    std::string out1 = exec_cmd("python3 I_quantum_algorithms/scripts/quantum_algorithms.py 2>/dev/null");
    if (out1.empty()) out1 = exec_cmd("python3 ../I_quantum_algorithms/scripts/quantum_algorithms.py 2>/dev/null");
    std::cout << out1 << "\n";

    print_csv_table("I_quantum_algorithms/results/quantum_algorithms_summary.csv", 
                    "Track I1: Quantum Gate Algorithms & Annealing Verification Summary");

    // 2. Run I2 QML vs Classical Classification Benchmark
    std::cout << "\n[Step 2] Executing Track I2: QML vs Classical Models on Make-Moons...\n";
    std::string out2 = exec_cmd("python3 I_quantum_algorithms/scripts/qml_benchmark.py 2>/dev/null");
    if (out2.empty()) out2 = exec_cmd("python3 ../I_quantum_algorithms/scripts/qml_benchmark.py 2>/dev/null");
    std::cout << out2 << "\n";

    print_csv_table("I_quantum_algorithms/results/qml_classification_comparison.csv",
                    "Track I2: Model Accuracy, Training Time, Parameters & Evaluations Comparison");

    std::cout << "\n>>> Track I1 & I2 Completed Successfully! <<<\n";
    return 0;
}
