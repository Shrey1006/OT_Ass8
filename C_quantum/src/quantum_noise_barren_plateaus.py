#!/usr/bin/env python3
"""
Track C4: Quantum Noise, Finite-Shot Effects, and Barren Plateaus
All experiments in this script are SIMULATED (no physical quantum hardware).
Components:
  1) Finite-shot projection noise (shots in {100, 1000, 10000}) vs. exact statevector.
  2) Hardware-like noise model (depolarizing gate noise + readout error via Qiskit Aer).
  3) Barren plateaus: gradient variance vs. qubit count n in {2, 4, 6, 8, 10} for global vs. local observables.
"""

import os
import time
import csv
import numpy as np
from qiskit import QuantumCircuit
from qiskit.quantum_info import Statevector, SparsePauliOp
from qiskit_aer import AerSimulator
from qiskit_aer.noise import NoiseModel, depolarizing_error, ReadoutError


def build_hardware_noise_model(p1=0.001, p2=0.01, p_meas01=0.02, p_meas10=0.015):
    """
    Constructs a realistic IBM-like hardware noise model:
      - 1-qubit depolarizing gate error: p1 (0.1%)
      - 2-qubit CNOT depolarizing error: p2 (1.0%)
      - Readout error: P(1|0) = 1.5%, P(0|1) = 2.0%
    """
    noise_model = NoiseModel()
    # 1-qubit gate error on ['ry', 'rx', 'rz', 'h', 'x']
    error_1q = depolarizing_error(p1, 1)
    noise_model.add_all_qubit_quantum_error(error_1q, ["ry", "rx", "rz", "h", "x"])

    # 2-qubit gate error on ['cx', 'cz']
    error_2q = depolarizing_error(p2, 2)
    noise_model.add_all_qubit_quantum_error(error_2q, ["cx", "cz"])

    # Readout error
    readout_err = ReadoutError([[1.0 - p_meas10, p_meas10],
                                [p_meas01, 1.0 - p_meas01]])
    noise_model.add_all_qubit_readout_error(readout_err)
    return noise_model


# ==============================================================================
# Experiment 1: Finite-Shot Projection Noise Scaling
# ==============================================================================
def run_finite_shot_experiment():
    print("-----------------------------------------------------------------------------------------")
    print("  [SIMULATED] Experiment 1: Finite-Shot Effects (Shots in {100, 1000, 10000})")
    print("-----------------------------------------------------------------------------------------")

    # System: 2-qubit Bell state |Phi+> = (|00> + |11>)/sqrt(2) measuring <Z0 Z1>
    # Exact analytical value is 1.0; and measuring <Z0> (exact value is 0.0)
    qc = QuantumCircuit(2, 2)
    qc.h(0)
    qc.cx(0, 1)
    qc.measure([0, 1], [0, 1])

    exact_z0z1 = 1.0
    exact_z0 = 0.0

    shot_counts = [100, 1000, 10000]
    num_seeds = 10
    base_seed = 500

    sim_ideal = AerSimulator()
    records = []

    print(f"{'Shots':<8} {'Seed':<6} {'Est <Z0Z1>':<14} {'Exact':<8} {'Error':<12} {'Est <Z0>':<12} {'Exact':<8} {'Error':<12}")
    print("-" * 88)

    summary_shots = {}

    for shots in shot_counts:
        errs_z0z1 = []
        errs_z0 = []
        vals_z0z1 = []
        vals_z0 = []

        for s_idx in range(num_seeds):
            seed = base_seed + s_idx
            res = sim_ideal.run(qc, shots=shots, seed_simulator=seed).result()
            counts = res.get_counts()

            # Expectation value of Z0 Z1: parity = +1 for '00' and '11', -1 for '01' and '10'
            # Note: Qiskit little-endian string: bit 0 is rightmost
            c00 = counts.get("00", 0)
            c11 = counts.get("11", 0)
            c01 = counts.get("01", 0)
            c10 = counts.get("10", 0)

            est_z0z1 = (c00 + c11 - c01 - c10) / float(shots)
            # Expectation value of Z0: +1 for bit 0 == 0 ('00', '10'), -1 for bit 0 == 1 ('01', '11')
            est_z0 = (c00 + c10 - c01 - c11) / float(shots)

            err_z0z1 = abs(est_z0z1 - exact_z0z1)
            err_z0 = abs(est_z0 - exact_z0)

            vals_z0z1.append(est_z0z1)
            vals_z0.append(est_z0)
            errs_z0z1.append(err_z0z1)
            errs_z0.append(err_z0)

            records.append({
                "mode": "SIMULATED_FINITE_SHOT",
                "shots": shots,
                "seed": seed,
                "est_z0z1": est_z0z1,
                "exact_z0z1": exact_z0z1,
                "err_z0z1": err_z0z1,
                "est_z0": est_z0,
                "exact_z0": exact_z0,
                "err_z0": err_z0
            })

            print(f"{shots:<8} {seed:<6} {est_z0z1:<14.4f} {exact_z0z1:<8.1f} {err_z0z1:<12.4f} {est_z0:<12.4f} {exact_z0:<8.1f} {err_z0:<12.4f}")

        std_z0z1 = np.std(vals_z0z1, ddof=1)
        mean_err_z0z1 = np.mean(errs_z0z1)
        std_z0 = np.std(vals_z0, ddof=1)
        mean_err_z0 = np.mean(errs_z0)

        summary_shots[shots] = {
            "mean_err_z0z1": mean_err_z0z1,
            "std_z0z1": std_z0z1,
            "mean_err_z0": mean_err_z0,
            "std_z0": std_z0,
            "theory_shot_noise": 1.0 / np.sqrt(shots)
        }
        print("-" * 88)
        print(f"  -> Shots={shots:<5}: Empirical Std = {std_z0:.4f} | Mean Error = {mean_err_z0:.4f} | Theory 1/sqrt(S) = {1.0/np.sqrt(shots):.4f}\n")

    csv_path = "C_quantum/results/finite_shot_benchmark.csv"
    with open(csv_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=[
            "mode", "shots", "seed", "est_z0z1", "exact_z0z1", "err_z0z1", "est_z0", "exact_z0", "err_z0"
        ])
        writer.writeheader()
        writer.writerows(records)

    return summary_shots


# ==============================================================================
# Experiment 2: Hardware-like Noise Model (Simulated)
# ==============================================================================
def run_noise_model_experiment():
    print("-----------------------------------------------------------------------------------------")
    print("  [SIMULATED] Experiment 2: Hardware-Like Noise Model (Depolarizing + Readout Error)")
    print("-----------------------------------------------------------------------------------------")

    noise_model = build_hardware_noise_model(p1=0.001, p2=0.01, p_meas01=0.02, p_meas10=0.015)
    sim_ideal = AerSimulator()
    sim_noisy = AerSimulator(noise_model=noise_model)

    # 1. Bell state fidelity test
    qc_bell = QuantumCircuit(2, 2)
    qc_bell.h(0)
    qc_bell.cx(0, 1)
    qc_bell.measure([0, 1], [0, 1])

    shots = 10000
    seeds = [601 + i for i in range(10)]

    records = []
    print(f"{'Condition':<24} {'Seed':<6} {'<Z0Z1>':<12} {'P(00)':<10} {'P(11)':<10} {'P_err (01+10)':<14} {'Bias from Exact':<16}")
    print("-" * 96)

    ideal_z0z1_list = []
    noisy_z0z1_list = []

    for seed in seeds:
        res_ideal = sim_ideal.run(qc_bell, shots=shots, seed_simulator=seed).result()
        c_id = res_ideal.get_counts()
        id_z0z1 = (c_id.get("00", 0) + c_id.get("11", 0) - c_id.get("01", 0) - c_id.get("10", 0)) / shots

        res_noisy = sim_noisy.run(qc_bell, shots=shots, seed_simulator=seed).result()
        c_no = res_noisy.get_counts()
        no_z0z1 = (c_no.get("00", 0) + c_no.get("11", 0) - c_no.get("01", 0) - c_no.get("10", 0)) / shots

        p00_no = c_no.get("00", 0) / shots
        p11_no = c_no.get("11", 0) / shots
        perr_no = (c_no.get("01", 0) + c_no.get("10", 0)) / shots
        bias = abs(no_z0z1 - 1.0)

        ideal_z0z1_list.append(id_z0z1)
        noisy_z0z1_list.append(no_z0z1)

        records.append({
            "mode": "SIMULATED_NOISE_MODEL",
            "seed": seed,
            "ideal_z0z1": id_z0z1,
            "noisy_z0z1": no_z0z1,
            "p00_noisy": p00_no,
            "p11_noisy": p11_no,
            "perr_noisy": perr_no,
            "bias_from_ideal": bias
        })

        print(f"{'Simulated Noisy Aer':<24} {seed:<6} {no_z0z1:<12.4f} {p00_no:<10.4f} {p11_no:<10.4f} {perr_no:<14.4f} {bias:<16.4f}")

    mean_noisy_z0z1 = np.mean(noisy_z0z1_list)
    std_noisy_z0z1 = np.std(noisy_z0z1_list, ddof=1)
    mean_bias = np.mean([r["bias_from_ideal"] for r in records])

    print("-" * 96)
    print(f"  -> Ideal Bell State <Z0Z1>: 1.0000 (Exact)")
    print(f"  -> Hardware-Simulated <Z0Z1>: {mean_noisy_z0z1:.4f} +/- {std_noisy_z0z1:.4f} (Mean Noise Bias = {mean_bias:.4f} due to 1% 2Q error & 2% readout error)\n")

    csv_path = "C_quantum/results/hardware_noise_benchmark.csv"
    with open(csv_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=[
            "mode", "seed", "ideal_z0z1", "noisy_z0z1", "p00_noisy", "p11_noisy", "perr_noisy", "bias_from_ideal"
        ])
        writer.writeheader()
        writer.writerows(records)

    return mean_noisy_z0z1, std_noisy_z0z1, mean_bias


# ==============================================================================
# Experiment 3: Barren Plateaus (Gradient Variance vs. Qubit Count 2 to 10)
# ==============================================================================
def run_barren_plateaus_experiment():
    print("-----------------------------------------------------------------------------------------")
    print("  [SIMULATED] Experiment 3: Barren Plateaus (Gradient Variance vs. Qubit Count n in [2, 10])")
    print("-----------------------------------------------------------------------------------------")

    qubit_counts = [2, 4, 6, 8, 10]
    num_samples = 40
    records = []

    print(f"{'Qubits (n)':<12} {'Hilbert Dim':<14} {'Var(d_th <Z_global>)':<24} {'Var(d_th <Z_local>)':<24} {'Ratio Local/Global':<18}")
    print("-" * 94)

    summary_bp = {}

    for n in qubit_counts:
        # Observable 1: Global observable Z^{\otimes n}
        H_global = SparsePauliOp.from_list([("Z" * n, 1.0)])

        # Observable 2: Local single-qubit observable Z_0
        z_chars = ["I"] * n
        z_chars[0] = "Z"
        H_local = SparsePauliOp.from_list([("".join(reversed(z_chars)), 1.0)])

        depth = n  # Circuit depth scales linearly with n (forming an approximate Haar 2-design)

        grads_global = []
        grads_local = []

        np.random.seed(700 + n)

        for s in range(num_samples):
            # Sample random parameters for all layers
            params = np.random.uniform(0.0, 2.0 * np.pi, (depth, n))

            def build_eval_circuit(shift):
                c = QuantumCircuit(n)
                for l in range(depth):
                    for q in range(n):
                        val = params[l, q]
                        if l == 0 and q == 0:
                            val += shift
                        c.ry(val, q)
                    for q in range(n - 1):
                        c.cx(q, q + 1)
                return c

            qc_plus = build_eval_circuit(+np.pi / 2.0)
            qc_minus = build_eval_circuit(-np.pi / 2.0)

            sv_plus = Statevector(qc_plus)
            sv_minus = Statevector(qc_minus)

            # Parameter-shift gradient for global observable
            g_glob = (sv_plus.expectation_value(H_global).real - sv_minus.expectation_value(H_global).real) / 2.0
            # Parameter-shift gradient for local observable
            g_loc = (sv_plus.expectation_value(H_local).real - sv_minus.expectation_value(H_local).real) / 2.0

            grads_global.append(g_glob)
            grads_local.append(g_loc)

        var_global = float(np.var(grads_global, ddof=1))
        var_local = float(np.var(grads_local, ddof=1))
        ratio = var_local / max(var_global, 1e-18)

        summary_bp[n] = {
            "var_global": var_global,
            "var_local": var_local,
            "dim": 2**n,
            "ratio": ratio
        }

        records.append({
            "mode": "SIMULATED_BARREN_PLATEAU",
            "n_qubits": n,
            "hilbert_dim": 2**n,
            "variance_global": var_global,
            "variance_local": var_local,
            "ratio_local_global": ratio
        })

        print(f"{n:<12} {2**n:<14} {var_global:<24.4e} {var_local:<24.4e} {ratio:<18.2f}")

    print("-" * 94)
    print("  -> Demonstration: Var(d_th <Z_global>) decays exponentially with qubit count n:")
    for n in qubit_counts:
        v = summary_bp[n]["var_global"]
        print(f"     n = {n:<2}: Var_global = {v:.2e} | 2^(-n) reference = {1.0/(2**n):.2e}")
    print("\n")

    csv_path = "C_quantum/results/barren_plateau_benchmark.csv"
    with open(csv_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=[
            "mode", "n_qubits", "hilbert_dim", "variance_global", "variance_local", "ratio_local_global"
        ])
        writer.writeheader()
        writer.writerows(records)

    return summary_bp


def main():
    print("=========================================================================================")
    print("  Track C4: Noise Models, Finite-Shot Effects & Barren Plateaus")
    print("  STATUS: ALL RESULTS ARE SIMULATED ON CLASSICAL HARDWARE (NOT PHYSICAL QPUs)")
    print("=========================================================================================\n")

    t0 = time.time()
    run_finite_shot_experiment()
    run_noise_model_experiment()
    run_barren_plateaus_experiment()
    t1 = time.time()

    print(f"[SUCCESS] All Track C4 benchmarks completed in {t1 - t0:.2f} seconds.")
    print("Artifacts generated:")
    print("  - C_quantum/results/finite_shot_benchmark.csv")
    print("  - C_quantum/results/hardware_noise_benchmark.csv")
    print("  - C_quantum/results/barren_plateau_benchmark.csv")


if __name__ == "__main__":
    main()
