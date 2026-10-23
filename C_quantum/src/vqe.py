#!/usr/bin/env python3
"""
Track C2: Variational Quantum Eigensolver (VQE) for Small Hamiltonians
Implements VQE for:
  1) Molecular H2 (STO-3G parity-reduced Hamiltonian at R=0.735 A)
  2) 2-Qubit Transverse-Field Ising Model (TFIM)
Compares VQE results against exact classical diagonalization over 10 deterministic seeds.
"""

import os
import time
import csv
import numpy as np
from qiskit import QuantumCircuit
from qiskit.quantum_info import SparsePauliOp, Statevector
from scipy.optimize import minimize


def get_h2_hamiltonian():
    """
    Returns the 2-qubit parity-reduced H2 molecular Hamiltonian at equilibrium (R = 0.735 A):
      H = -1.052373 II + 0.397937 ZI - 0.397937 IZ - 0.011280 ZZ + 0.180931 XX
    Source: O'Malley et al., Phys. Rev. X 6, 031007 (2016).
    """
    coeffs = [-1.05237324, 0.39793742, -0.39793742, -0.01128010, 0.18093119]
    paulis = ["II", "ZI", "IZ", "ZZ", "XX"]
    return SparsePauliOp.from_list(list(zip(paulis, coeffs))), "Molecular H2 (R=0.735 A)"


def get_ising_hamiltonian(J=1.0, g=0.8):
    """
    Returns the 2-qubit Transverse-Field Ising Model (TFIM) Hamiltonian:
      H = -J Z0 Z1 - g (X0 + X1)
    """
    paulis = ["ZZ", "IX", "XI"]
    coeffs = [-J, -g, -g]
    return SparsePauliOp.from_list(list(zip(paulis, coeffs))), f"2-Qubit TFIM (J={J}, g={g})"


def exact_diagonalization(hamiltonian):
    """Computes exact eigenvalues and ground state energy via full Hermitian matrix diagonalization."""
    matrix = hamiltonian.to_matrix()
    eigenvalues, eigenvectors = np.linalg.eigh(matrix)
    return eigenvalues, eigenvectors


def create_vqe_ansatz(params):
    """
    Hardware-efficient 2-qubit RealAmplitudes ansatz:
      |psi(theta)> = RY(theta_2, theta_3) * CNOT(0, 1) * RY(theta_0, theta_1) |00>
    Provides complete real-state expressibility in R^4.
    """
    qc = QuantumCircuit(2)
    qc.ry(params[0], 0)
    qc.ry(params[1], 1)
    qc.cx(0, 1)
    qc.ry(params[2], 0)
    qc.ry(params[3], 1)
    return qc


def run_vqe_experiment():
    csv_path = "C_quantum/results/vqe_benchmark.csv"
    os.makedirs(os.path.dirname(csv_path), exist_ok=True)

    hamiltonians = [get_h2_hamiltonian(), get_ising_hamiltonian()]
    optimizers = ["BFGS", "COBYLA", "SLSQP"]
    num_seeds = 10
    base_seed = 100

    results_records = []

    print("=========================================================================================")
    print("   Track C2: Variational Quantum Eigensolver (VQE) vs. Exact Diagonalization")
    print("=========================================================================================\n")

    summary_records = []

    for H, name in hamiltonians:
        exact_eigs, _ = exact_diagonalization(H)
        e0_exact = exact_eigs[0]

        print(f"--- Benchmark System: {name} ---")
        print(f"Exact Spectrum: {np.round(exact_eigs, 6)}")
        print(f"Exact Ground State Energy E0: {e0_exact:.12f} Hartree / units\n")

        print(f"{'Optimizer':<10} {'Seed':<6} {'VQE Energy':<18} {'Exact E0':<18} {'Energy Error':<16} {'N_eval':<8} {'Time (ms)':<10}")
        print("-" * 88)

        for opt_name in optimizers:
            vqe_energies = []
            energy_errors = []
            n_evals = []
            runtimes_ms = []

            for s_idx in range(num_seeds):
                seed = base_seed + s_idx
                np.random.seed(seed)
                init_params = np.random.uniform(-np.pi, np.pi, 4)

                eval_count = 0

                def cost_func(params):
                    nonlocal eval_count
                    eval_count += 1
                    qc = create_vqe_ansatz(params)
                    sv = Statevector(qc)
                    return sv.expectation_value(H).real

                t_start = time.perf_counter()
                if opt_name == "COBYLA":
                    res = minimize(cost_func, init_params, method="COBYLA", options={"maxiter": 300, "tol": 1e-7})
                elif opt_name == "BFGS":
                    res = minimize(cost_func, init_params, method="BFGS", options={"maxiter": 150, "gtol": 1e-7})
                elif opt_name == "SLSQP":
                    res = minimize(cost_func, init_params, method="SLSQP", options={"maxiter": 150, "ftol": 1e-9})
                t_end = time.perf_counter()

                runtime_ms = (t_end - t_start) * 1000.0
                vqe_e = res.fun
                err = abs(vqe_e - e0_exact)

                vqe_energies.append(vqe_e)
                energy_errors.append(err)
                n_evals.append(eval_count)
                runtimes_ms.append(runtime_ms)

                results_records.append({
                    "system": name,
                    "optimizer": opt_name,
                    "seed": seed,
                    "vqe_energy": vqe_e,
                    "exact_e0": e0_exact,
                    "absolute_error": err,
                    "n_evals": eval_count,
                    "runtime_ms": runtime_ms
                })

                print(f"{opt_name:<10} {seed:<6} {vqe_e:<18.10f} {e0_exact:<18.10f} {err:<16.2e} {eval_count:<8} {runtime_ms:<10.2f}")

            mean_e = np.mean(vqe_energies)
            std_e = np.std(vqe_energies, ddof=1)
            mean_err = np.mean(energy_errors)
            std_err = np.std(energy_errors, ddof=1)
            mean_eval = np.mean(n_evals)
            mean_time = np.mean(runtimes_ms)

            summary_records.append({
                "system": name,
                "optimizer": opt_name,
                "exact_e0": e0_exact,
                "mean_e": mean_e,
                "std_e": std_e,
                "mean_err": mean_err,
                "std_err": std_err,
                "mean_eval": mean_eval,
                "mean_time_ms": mean_time
            })
            print("-" * 88)
            print(f"  -> {opt_name} Summary: VQE E = {mean_e:.10f} +/- {std_e:.2e} | Mean Error = {mean_err:.2e} +/- {std_err:.2e} | Evaluations = {mean_eval:.1f}\n")

    # Export to CSV
    with open(csv_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=[
            "system", "optimizer", "seed", "vqe_energy", "exact_e0",
            "absolute_error", "n_evals", "runtime_ms"
        ])
        writer.writeheader()
        writer.writerows(results_records)

    print(f"[SUCCESS] VQE benchmark complete. Results saved to: {csv_path}\n")

    print("=========================================================================================")
    print("  Overall VQE vs. Exact Diagonalization Summary (10 Seeds per Optimizer)")
    print("=========================================================================================")
    print(f"{'System':<24} {'Optimizer':<10} {'Exact E0':<16} {'Mean VQE E':<18} {'Mean Abs Error':<16} {'Mean Evals':<10}")
    print("-" * 96)
    for s in summary_records:
        e0_str = f"{s['exact_e0']:.8f}"
        vqe_str = f"{s['mean_e']:.8f}"
        err_str = f"{s['mean_err']:.2e} +/- {s['std_err']:.1e}"
        print(f"{s['system']:<24} {s['optimizer']:<10} {e0_str:<16} {vqe_str:<18} {err_str:<16} {s['mean_eval']:<10.1f}")
    print("=========================================================================================\n")


if __name__ == "__main__":
    run_vqe_experiment()
