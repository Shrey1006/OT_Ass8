#!/usr/bin/env python3
"""
Track I3: Grover's Search vs Classical Linear Search Scaling Benchmark
and Noisy Simulation Experiment.

Outputs:
  - I_quantum_algorithms/results/grover_scaling_comparison.csv
  - I_quantum_algorithms/results/grover_noise_experiment.csv
"""

import math
import time
import os
import sys
import numpy as np

# Qiskit 2.x
from qiskit import QuantumCircuit, QuantumRegister, ClassicalRegister, transpile
from qiskit_aer import AerSimulator
from qiskit_aer.noise import NoiseModel, depolarizing_error

SIMULATOR = AerSimulator()

# ==============================================================================
# 1. GROVER CIRCUIT BUILDER (N = 2^n)
# ==============================================================================
def build_grover_circuit(n, target_int):
    """
    Builds an n-qubit Grover search circuit targeting basis state target_int.
    """
    target_bin = format(target_int, f"0{n}b")
    qr = QuantumRegister(n, "q")
    cr = ClassicalRegister(n, "c")
    qc = QuantumCircuit(qr, cr)

    # 1. Equal superposition
    for i in range(n):
        qc.h(qr[i])

    # Optimal iterations R = floor(pi/4 * sqrt(N))
    N = 2 ** n
    iterations = max(1, int(math.floor((math.pi / 4.0) * math.sqrt(N))))

    for _ in range(iterations):
        qc.barrier()
        # --- Oracle O_w ---
        # Flip bits where target_bin is '0'
        for i in range(n):
            if target_bin[n - 1 - i] == '0':
                qc.x(qr[i])

        # Multi-controlled Z: H on target qubit, MCX, H on target qubit
        qc.h(qr[n - 1])
        if n == 2:
            qc.cx(qr[0], qr[1])
        elif n == 3:
            qc.ccx(qr[0], qr[1], qr[2])
        else:
            qc.mcx(list(qr[:n - 1]), qr[n - 1])
        qc.h(qr[n - 1])

        # Uncompute
        for i in range(n):
            if target_bin[n - 1 - i] == '0':
                qc.x(qr[i])

        qc.barrier()
        # --- Diffusion Operator D ---
        for i in range(n):
            qc.h(qr[i])
        for i in range(n):
            qc.x(qr[i])

        qc.h(qr[n - 1])
        if n == 2:
            qc.cx(qr[0], qr[1])
        elif n == 3:
            qc.ccx(qr[0], qr[1], qr[2])
        else:
            qc.mcx(list(qr[:n - 1]), qr[n - 1])
        qc.h(qr[n - 1])

        for i in range(n):
            qc.x(qr[i])
        for i in range(n):
            qc.h(qr[i])

    qc.barrier()
    for i in range(n):
        qc.measure(qr[i], cr[i])

    return qc, iterations, target_bin

# ==============================================================================
# 2. SCALING COMPARISON: GROVER VS CLASSICAL LINEAR SEARCH
# ==============================================================================
def run_scaling_benchmark():
    print("--- [Part 1] Grover vs Classical Linear Search Scaling Benchmark ---")
    qubit_sizes = [3, 4, 5, 6, 7, 8]  # N = 8, 16, 32, 64, 128, 256
    scaling_records = []

    for n in qubit_sizes:
        N = 2 ** n
        target_int = int(N * 0.65)  # Deterministic target at 65% of array
        qc, grover_queries, target_bin = build_grover_circuit(n, target_int)

        # Transpile to elementary basis {u, cx} to count real physical gates
        tqc = transpile(qc, SIMULATOR, basis_gates=["u", "cx"])
        gate_counts = tqc.count_ops()
        total_quantum_gates = sum(gate_counts.values()) - gate_counts.get("measure", 0) - gate_counts.get("barrier", 0)
        cx_gates = gate_counts.get("cx", 0)
        depth = tqc.depth()

        # Simulated Grover execution
        t0 = time.perf_counter()
        sim_res = SIMULATOR.run(tqc, shots=1024).result().get_counts()
        grover_sim_time_ms = (time.perf_counter() - t0) * 1000.0

        success_shots = sim_res.get(target_bin, 0)
        grover_acc = (success_shots / 1024.0) * 100.0

        # Estimated physical superconducting hardware execution time:
        # 1-qubit gate = 20 ns, 2-qubit (CX) gate = 100 ns
        u_gates = gate_counts.get("u", 0)
        phys_hardware_time_us = (u_gates * 0.02 + cx_gates * 0.10)  # in microseconds

        # Classical Linear Search
        # In-memory array of N items
        data = list(range(N))
        # Measure linear search wall-clock time over 10,000 iterations for precision
        t_start = time.perf_counter()
        reps = 10000
        classical_queries = 0
        for _ in range(reps):
            for idx, val in enumerate(data):
                if val == target_int:
                    classical_queries = idx + 1
                    break
        t_end = time.perf_counter()
        classical_wall_clock_us = ((t_end - t_start) / reps) * 1e6
        classical_comparisons = classical_queries

        # Classical gate / ALU ops: 1 comparison + 1 index increment per element
        classical_alu_ops = classical_comparisons * 2

        scaling_records.append({
            "n_qubits": n,
            "search_space_N": N,
            "classical_queries": classical_queries,
            "grover_queries": grover_queries,
            "query_speedup": round(classical_queries / float(grover_queries), 2),
            "classical_alu_ops": classical_alu_ops,
            "quantum_gates": total_quantum_gates,
            "cx_gates": cx_gates,
            "circuit_depth": depth,
            "grover_success_pct": round(grover_acc, 2),
            "classical_wall_clock_us": round(classical_wall_clock_us, 3),
            "simulated_quantum_ms": round(grover_sim_time_ms, 2),
            "est_physical_hardware_us": round(phys_hardware_time_us, 3)
        })

        print(f"  N={N:3d} (n={n}) | Queries: Classical={classical_queries:3d} vs Grover={grover_queries:2d} ({classical_queries/grover_queries:4.1f}x speedup) | "
              f"Gates: Classical={classical_alu_ops:3d} vs Quantum={total_quantum_gates:4d} (CX={cx_gates:3d}) | "
              f"Clock: Classical={classical_wall_clock_us:.3f} us vs HW_est={phys_hardware_time_us:.2f} us")

    return scaling_records

# ==============================================================================
# 3. NOISY SIMULATION EXPERIMENT
# ==============================================================================
def run_noise_experiment():
    print("\n--- [Part 2] Grover Noisy Simulation Experiment (n=3, N=8, Depolarizing Noise) ---")
    n = 3
    N = 8
    target_int = 5  # |101>
    qc, iterations, target_bin = build_grover_circuit(n, target_int)

    # Error rates to test
    noise_rates = [0.0, 0.001, 0.005, 0.010, 0.020, 0.030, 0.050]
    noise_records = []

    for p1 in noise_rates:
        p2 = p1 * 2.0  # 2-qubit gate error is typically 2x - 5x higher than 1-qubit error
        
        noise_model = NoiseModel()
        if p1 > 0.0:
            err_1q = depolarizing_error(p1, 1)
            err_2q = depolarizing_error(min(0.75, p2), 2)
            noise_model.add_all_qubit_quantum_error(err_1q, ["u"])
            noise_model.add_all_qubit_quantum_error(err_2q, ["cx"])

        # Transpile to physical basis {u, cx} so noise model applies directly to native gates
        tqc = transpile(qc, SIMULATOR, basis_gates=["u", "cx"])
        shots = 4096
        res = SIMULATOR.run(tqc, noise_model=noise_model, shots=shots).result().get_counts()

        target_counts = res.get(target_bin, 0)
        success_prob = (target_counts / float(shots)) * 100.0
        random_baseline = (1.0 / N) * 100.0  # 12.5%

        noise_records.append({
            "one_qubit_noise_pct": round(p1 * 100.0, 2),
            "two_qubit_noise_pct": round(p2 * 100.0, 2),
            "success_probability_pct": round(success_prob, 2),
            "random_guess_baseline_pct": round(random_baseline, 2),
            "fidelity_advantage_pct": round(success_prob - random_baseline, 2)
        })

        print(f"  Noise p_1q={p1*100:4.1f}%, p_2q={p2*100:4.1f}% | Success Prob: {success_prob:5.2f}% | "
              f"Baseline: {random_baseline:.1f}% | Advantage: +{success_prob - random_baseline:5.2f}%")

    return noise_records

# ==============================================================================
# MAIN DRIVER
# ==============================================================================
def main():
    print("=========================================================================")
    print(" Track I3: Grover vs Classical Linear Search & Noisy Simulation Study   ")
    print("=========================================================================\n")

    scaling_records = run_scaling_benchmark()
    noise_records = run_noise_experiment()

    # Save Scaling CSV
    out_dir = os.path.join(os.path.dirname(__file__), "..", "results")
    os.makedirs(out_dir, exist_ok=True)
    scaling_csv = os.path.join(out_dir, "grover_scaling_comparison.csv")
    with open(scaling_csv, "w") as f:
        f.write("n_qubits,search_space_N,classical_queries,grover_queries,query_speedup,"
                "classical_alu_ops,quantum_gates,cx_gates,circuit_depth,grover_success_pct,"
                "classical_wall_clock_us,simulated_quantum_ms,est_physical_hardware_us\n")
        for r in scaling_records:
            f.write(f"{r['n_qubits']},{r['search_space_N']},{r['classical_queries']},{r['grover_queries']},{r['query_speedup']},"
                    f"{r['classical_alu_ops']},{r['quantum_gates']},{r['cx_gates']},{r['circuit_depth']},{r['grover_success_pct']},"
                    f"{r['classical_wall_clock_us']},{r['simulated_quantum_ms']},{r['est_physical_hardware_us']}\n")
    print(f"\n[INFO] Scaling comparison saved to: {scaling_csv}")

    # Save Noise CSV
    noise_csv = os.path.join(out_dir, "grover_noise_experiment.csv")
    with open(noise_csv, "w") as f:
        f.write("one_qubit_noise_pct,two_qubit_noise_pct,success_probability_pct,random_guess_baseline_pct,fidelity_advantage_pct\n")
        for r in noise_records:
            f.write(f"{r['one_qubit_noise_pct']},{r['two_qubit_noise_pct']},{r['success_probability_pct']},{r['random_guess_baseline_pct']},{r['fidelity_advantage_pct']}\n")
    print(f"[INFO] Noise experiment saved to: {noise_csv}")

    print("\n=========================================================================")
    print(" >>> Track I3 Grover Search & Noise Experiments Completed! <<<           ")
    print("=========================================================================")

if __name__ == "__main__":
    main()
