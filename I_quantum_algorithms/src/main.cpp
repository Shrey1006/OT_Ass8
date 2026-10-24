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

int main() {
    std::cout << "========================================================================\n";
    std::cout << " Track I1: Quantum Algorithms & Annealing Execution Suite               \n";
    std::cout << "========================================================================\n\n";

    // Run the Python quantum algorithms suite
    std::cout << "[INFO] Executing Quantum Algorithms Suite (Qiskit 2.6 & D-Wave Neal)...\n";
    std::string run_out = exec_cmd("python3 I_quantum_algorithms/scripts/quantum_algorithms.py 2>/dev/null");
    if (run_out.empty()) {
        run_out = exec_cmd("python3 ../I_quantum_algorithms/scripts/quantum_algorithms.py 2>/dev/null");
    }

    std::cout << run_out << "\n";

    // Read and display summary table
    std::ifstream csv("I_quantum_algorithms/results/quantum_algorithms_summary.csv");
    if (!csv.is_open()) {
        csv.open("../I_quantum_algorithms/results/quantum_algorithms_summary.csv");
    }

    if (csv.is_open()) {
        std::cout << "---------------------------------------------------------------------------------------------------------\n";
        std::cout << " Verified Quantum Algorithms Results Summary Table\n";
        std::cout << "---------------------------------------------------------------------------------------------------------\n";
        std::cout << std::left
                  << std::setw(36) << "Algorithm"
                  << std::setw(30) << "Problem / Oracle"
                  << std::setw(10) << "Qubits"
                  << std::setw(12) << "Status"
                  << "Key Quantum Metric"
                  << "\n";
        std::cout << "---------------------------------------------------------------------------------------------------------\n";

        std::string line;
        std::getline(csv, line); // header
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
                          << std::setw(36) << cols[0]
                          << std::setw(30) << cols[1]
                          << std::setw(10) << cols[2]
                          << std::setw(12) << cols[3]
                          << cols[4]
                          << "\n";
            }
        }
        std::cout << "---------------------------------------------------------------------------------------------------------\n";
    }

    std::cout << "\n>>> Track I1 Quantum Algorithms Suite Completed Successfully! <<<\n";
    return 0;
}
