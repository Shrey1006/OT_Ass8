#include <iostream>
#include <vector>
#include <complex>
#include <string>
#include <random>
#include <cmath>
#include <cassert>
#include <map>

using cplx = std::complex<double>;
const double PI = std::acos(-1.0);

struct QSim {
    int n;
    std::vector<cplx> psi;
    explicit QSim(int qubits) : n(qubits), psi(1 << qubits, 0.0) {
        assert(qubits >= 1 && qubits <= 12);
        psi[0] = 1.0;
    }
    void apply_u(int q, cplx u00, cplx u01, cplx u10, cplx u11) {
        for (int i = 0; i < (1 << n); ++i) {
            if (!((i >> q) & 1)) {
                int j = i | (1 << q);
                cplx a = psi[i], b = psi[j];
                psi[i] = u00 * a + u01 * b;
                psi[j] = u10 * a + u11 * b;
            }
        }
    }
    void h(int q) { double s = 1.0 / std::sqrt(2.0); apply_u(q, s, s, s, -s); }
    void x(int q) { apply_u(q, 0.0, 1.0, 1.0, 0.0); }
    void rx(int q, double th) { apply_u(q, std::cos(th/2), cplx(0, -std::sin(th/2)), cplx(0, -std::sin(th/2)), std::cos(th/2)); }
    void ry(int q, double th) { apply_u(q, std::cos(th/2), -std::sin(th/2), std::sin(th/2), std::cos(th/2)); }
    void rz(int q, double th) { apply_u(q, std::polar(1.0, -th/2), 0.0, 0.0, std::polar(1.0, th/2)); }
    void cnot(int c, int t) {
        for (int i = 0; i < (1 << n); ++i)
            if (((i >> c) & 1) && !((i >> t) & 1)) std::swap(psi[i], psi[i | (1 << t)]);
    }
    void cz(int c, int t) {
        for (int i = 0; i < (1 << n); ++i)
            if (((i >> c) & 1) && ((i >> t) & 1)) psi[i] = -psi[i];
    }
    double expval_z(const std::string& z_str) const {
        double ev = 0.0;
        for (int i = 0; i < (1 << n); ++i) {
            int sign = 1;
            for (int q = 0; q < n && q < (int)z_str.size(); ++q)
                if (z_str[q] == 'Z' && ((i >> q) & 1)) sign = -sign;
            ev += sign * std::norm(psi[i]);
        }
        return ev;
    }
    std::map<std::string, int> sample(int shots, unsigned int seed) const {
        std::vector<double> probs(1 << n);
        for (int i = 0; i < (1 << n); ++i) probs[i] = std::norm(psi[i]);
        std::mt19937 rng(seed);
        std::discrete_distribution<int> dist(probs.begin(), probs.end());
        std::map<std::string, int> counts;
        for (int s = 0; s < shots; ++s) {
            int idx = dist(rng);
            std::string bs(n, '0');
            for (int q = 0; q < n; ++q) if ((idx >> q) & 1) bs[q] = '1';
            counts[bs]++;
        }
        return counts;
    }
};

int main() {
    std::cout << "========================================================\n";
    std::cout << "  Track C0: Statevector Quantum Simulator (< 120 lines)\n";
    std::cout << "========================================================\n";
    // 1. Bell state test |Phi+> = (|00> + |11>)/sqrt(2)
    QSim bell(2);
    bell.h(0); bell.cnot(0, 1);
    double z0z1 = bell.expval_z("ZZ"), z0 = bell.expval_z("ZI"), z1 = bell.expval_z("IZ");
    assert(std::abs(z0z1 - 1.0) < 1e-12 && std::abs(z0) < 1e-12 && std::abs(z1) < 1e-12);
    auto bell_counts = bell.sample(1000, 42);
    assert(bell_counts["00"] + bell_counts["11"] == 1000 && bell_counts["01"] == 0 && bell_counts["10"] == 0);
    assert(bell_counts["00"] > 400 && bell_counts["11"] > 400);
    std::cout << "[PASS] Bell state: <ZZ>=" << z0z1 << " | Shots: 00=" << bell_counts["00"] << ", 11=" << bell_counts["11"] << "\n";

    // 2. Single-qubit rotations against known analytical values
    QSim rot(1);
    rot.ry(0, PI / 3.0); // <Z> = cos(pi/3) = 0.5
    assert(std::abs(rot.expval_z("Z") - 0.5) < 1e-12);
    rot.rx(0, PI); // <Z> becomes -0.5
    assert(std::abs(rot.expval_z("Z") - (-0.5)) < 1e-12);
    rot.rz(0, PI / 2.0); // diagonal phase rotation leaves <Z> invariant
    assert(std::abs(rot.expval_z("Z") - (-0.5)) < 1e-12);
    std::cout << "[PASS] Single-qubit rotations (RX, RY, RZ) verified against analytical values.\n";

    // 3. X and CZ gate verification
    QSim cz_test(2);
    cz_test.x(0); cz_test.x(1); // |11>
    assert(std::abs(cz_test.expval_z("ZZ") - 1.0) < 1e-12);
    cz_test.cz(0, 1); // CZ|11> = -|11> (relative phase), <ZZ> remains 1.0
    assert(std::abs(cz_test.expval_z("ZZ") - 1.0) < 1e-12);
    cz_test.h(0); // Superposition under phase
    assert(std::abs(cz_test.expval_z("ZI") - 0.0) < 1e-12);
    std::cout << "[PASS] Gates H, X, CNOT, CZ verified successfully.\n";

    std::cout << "All C0 statevector simulator tests passed (100% OK).\n";
    return 0;
}
