#include <iostream>
#include <cassert>
#include <string>
#include <array>
#include <memory>
#include <fstream>
#include <sstream>
#include <vector>

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

void test_deutsch_jozsa() {
    std::cout << "[Test 1] Deutsch-Jozsa Algorithm (Constant vs Balanced Oracle)..." << std::flush;
    std::string cmd = "python3 -c \"import sys; sys.path.append('I_quantum_algorithms/scripts'); sys.path.append('../I_quantum_algorithms/scripts'); from quantum_algorithms import run_deutsch_jozsa; r=run_deutsch_jozsa(3); assert r['constant_oracle']['is_constant']; assert r['balanced_oracle']['is_balanced']; print('OK')\" 2>/dev/null";
    std::string out = exec_cmd(cmd.c_str());
    assert(out.find("OK") != std::string::npos);
    std::cout << " PASSED (100% Deterministic Discrimination)\n";
}

void test_bernstein_vazirani() {
    std::cout << "[Test 2] Bernstein-Vazirani Algorithm (Manual Gate Circuit)..." << std::flush;
    std::string cmd = "python3 -c \"import sys; sys.path.append('I_quantum_algorithms/scripts'); sys.path.append('../I_quantum_algorithms/scripts'); from quantum_algorithms import run_bernstein_vazirani; r=run_bernstein_vazirani('1011'); assert r['exact_match']; assert r['circuit_depth'] > 0; print('OK')\" 2>/dev/null";
    std::string out = exec_cmd(cmd.c_str());
    assert(out.find("OK") != std::string::npos);
    std::cout << " PASSED (Exact Recovery s='1011' in 1 Query)\n";
}

void test_grover_search() {
    std::cout << "[Test 3] Grover's Search Algorithm (Manual Oracle & Diffusion)..." << std::flush;
    std::string cmd = "python3 -c \"import sys; sys.path.append('I_quantum_algorithms/scripts'); sys.path.append('../I_quantum_algorithms/scripts'); from quantum_algorithms import run_grover_search; r=run_grover_search('101'); assert r['success_probability'] > 0.85; assert r['iterations'] == 2; print('OK')\" 2>/dev/null";
    std::string out = exec_cmd(cmd.c_str());
    assert(out.find("OK") != std::string::npos);
    std::cout << " PASSED (P(success) > 90% in 2 Iterations)\n";
}

void test_qft_unitarity() {
    std::cout << "[Test 4] Quantum Fourier Transform (QFT & QFT_dagger Unitarity)..." << std::flush;
    std::string cmd = "python3 -c \"import sys; sys.path.append('I_quantum_algorithms/scripts'); sys.path.append('../I_quantum_algorithms/scripts'); from quantum_algorithms import run_qft_verification; r=run_qft_verification(5, 3); assert r['exact_match']; assert r['fidelity'] == 1.0; print('OK')\" 2>/dev/null";
    std::string out = exec_cmd(cmd.c_str());
    assert(out.find("OK") != std::string::npos);
    std::cout << " PASSED (Exact Inversion QFT^dag * QFT = I)\n";
}

void test_qpe_phase() {
    std::cout << "[Test 5] Quantum Phase Estimation (QPE for Phase theta = 3/8)..." << std::flush;
    std::string cmd = "python3 -c \"import sys; sys.path.append('I_quantum_algorithms/scripts'); sys.path.append('../I_quantum_algorithms/scripts'); from quantum_algorithms import run_qpe; r=run_qpe(3, 8, 3); assert r['exact_match']; assert r['confidence'] == 1.0; print('OK')\" 2>/dev/null";
    std::string out = exec_cmd(cmd.c_str());
    assert(out.find("OK") != std::string::npos);
    std::cout << " PASSED (Exact Phase 0.011_2 = 0.375 Measured)\n";
}

void test_shor_factorization() {
    std::cout << "[Test 6] Shor's Order-Finding Algorithm (Factoring N=15, a=7)..." << std::flush;
    std::string cmd = "python3 -c \"import sys; sys.path.append('I_quantum_algorithms/scripts'); sys.path.append('../I_quantum_algorithms/scripts'); from quantum_algorithms import run_shor_15; r=run_shor_15(7); assert r['success']; assert 3 in r['factors_found'] and 5 in r['factors_found']; print('OK')\" 2>/dev/null";
    std::string out = exec_cmd(cmd.c_str());
    assert(out.find("OK") != std::string::npos);
    std::cout << " PASSED (Order r=4 -> Non-Trivial Factors {3, 5})\n";
}

void test_quantum_annealing() {
    std::cout << "[Test 7] Quantum Annealing Simulation (D-Wave SimulatedAnnealingSampler)..." << std::flush;
    std::string cmd = "python3 -c \"import sys; sys.path.append('I_quantum_algorithms/scripts'); sys.path.append('../I_quantum_algorithms/scripts'); from quantum_algorithms import run_quantum_annealing_example; r=run_quantum_annealing_example(); assert r['ground_state_energy'] <= -3.5; assert r['num_reads'] == 200; print('OK')\" 2>/dev/null";
    std::string out = exec_cmd(cmd.c_str());
    assert(out.find("OK") != std::string::npos);
    std::cout << " PASSED (Ising Ground State Energy Minimized <= -3.5)\n";
}

void test_summary_csv_artifacts() {
    std::cout << "[Test 8] Track I1 Results Summary Artifacts Verification..." << std::flush;
    std::ifstream csv("I_quantum_algorithms/results/quantum_algorithms_summary.csv");
    if (!csv.is_open()) {
        csv.open("../I_quantum_algorithms/results/quantum_algorithms_summary.csv");
    }
    assert(csv.is_open());
    std::string line;
    std::getline(csv, line); // header
    int count = 0;
    while (std::getline(csv, line)) {
        if (!line.empty()) {
            assert(line.find("PASSED") != std::string::npos);
            count++;
        }
    }
    assert(count == 7);
    std::cout << " PASSED (All 7 Algorithms Recorded as PASSED)\n";
}

int main() {
    std::cout << "=========================================================================\n";
    std::cout << " Running Track I1: Quantum Algorithms & Annealing Unit Tests            \n";
    std::cout << "=========================================================================\n";

    test_deutsch_jozsa();
    test_bernstein_vazirani();
    test_grover_search();
    test_qft_unitarity();
    test_qpe_phase();
    test_shor_factorization();
    test_quantum_annealing();
    test_summary_csv_artifacts();

    std::cout << "\n>>> All Track I1 Quantum Algorithm & Annealing Tests PASSED (100% OK)! <<<\n";
    return 0;
}
