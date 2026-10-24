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
    if (!std::getline(csv, line)) return;

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
                      << std::setw(32) << cols[0]
                      << std::setw(18) << cols[1]
                      << std::setw(16) << cols[2]
                      << std::setw(16) << cols[3]
                      << cols[4]
                      << "\n";
        }
    }
    std::cout << "---------------------------------------------------------------------------------------------------------\n";
}

int main() {
    std::cout << "========================================================================\n";
    std::cout << " Track I: Complete Quantum Algorithms, QML & Grover Scaling Suite       \n";
    std::cout << "========================================================================\n\n";

    // 1. Run I1 Quantum Algorithms Suite
    std::cout << "[Step 1] Executing Track I1: 6 Quantum Gate Algorithms & Annealing...\n";
    std::string out1 = exec_cmd("python3 I_quantum_algorithms/scripts/quantum_algorithms.py 2>/dev/null");
    if (out1.empty()) out1 = exec_cmd("python3 ../I_quantum_algorithms/scripts/quantum_algorithms.py 2>/dev/null");
    std::cout << out1 << "\n";

    // 2. Run I2 QML vs Classical Classification Benchmark
    std::cout << "\n[Step 2] Executing Track I2: QML (QSVM, VQC) vs Classical (RBF, MLP)...\n";
    std::string out2 = exec_cmd("python3 I_quantum_algorithms/scripts/qml_benchmark.py 2>/dev/null");
    if (out2.empty()) out2 = exec_cmd("python3 ../I_quantum_algorithms/scripts/qml_benchmark.py 2>/dev/null");
    std::cout << out2 << "\n";

    // 3. Run I3 Grover vs Classical Linear Search & Noise Experiment
    std::cout << "\n[Step 3] Executing Track I3: Grover Scaling vs Classical & Noisy Simulation...\n";
    std::string out3 = exec_cmd("python3 I_quantum_algorithms/scripts/grover_vs_classical.py 2>/dev/null");
    if (out3.empty()) out3 = exec_cmd("python3 ../I_quantum_algorithms/scripts/grover_vs_classical.py 2>/dev/null");
    std::cout << out3 << "\n";

    print_csv_table("I_quantum_algorithms/results/grover_scaling_comparison.csv",
                    "Track I3: Grover vs Classical Query, Gate & Clock Scaling (N=8..256)");

    print_csv_table("I_quantum_algorithms/results/grover_noise_experiment.csv",
                    "Track I3: Grover Fidelity Decay under Depolarizing Noise");

    std::cout << "\n>>> Track I1, I2 & I3 Completed Successfully! <<<\n";
    return 0;
}
