#!/usr/bin/env python3
"""
Track C1: Quantum Approximate Optimization Algorithm (QAOA) for Max-Cut
Solves the Max-Cut QUBO generated in Track B (results/maxcut_qubo.json)
for p in {1, 2, 3} across 10 deterministic seeds using Qiskit Statevector simulator.
"""

import os
import json
import time
import csv
import numpy as np
from qiskit import QuantumCircuit
from qiskit.quantum_info import SparsePauliOp, Statevector
from scipy.optimize import minimize


def load_maxcut_instance(json_candidates):
    """Loads the Max-Cut problem graph and Ising parameters from Track B JSON."""
    for path in json_candidates:
        if os.path.exists(path):
            with open(path, "r") as f:
                data = json.load(f)
            print(f"[INFO] Loaded Max-Cut instance from: {path}")
            return data
    raise FileNotFoundError(f"Could not find Max-Cut JSON in any of: {json_candidates}")


def build_cost_hamiltonian(data):
    """
    Constructs the problem Hamiltonian H_C in terms of Pauli-Z operators:
      H_C = sum_{(u,v) in E} J_{uv} Z_u Z_v + offset * I
    where J_{uv} = 0.5 * w_{uv} and offset = -0.5 * sum w_{uv}.
    Minimizing <H_C> maximizes Cut(x) = -<H_C>.
    """
    n = data["num_nodes"]
    edges = data["ising_couplings"]
    offset = data["ising_offset"]

    pauli_list = []
    for e in edges:
        u, v, J = e["u"], e["v"], e["J"]
        # In Qiskit's little-endian indexing, qubit 0 is the rightmost character
        z_chars = ["I"] * n
        z_chars[n - 1 - u] = "Z"
        z_chars[n - 1 - v] = "Z"
        pauli_list.append(("".join(z_chars), float(J)))

    pauli_list.append(("I" * n, float(offset)))
    return SparsePauliOp.from_list(pauli_list)


def create_qaoa_circuit(n, edges, params, p):
    """
    Constructs the p-layer QAOA variational ansatz:
      |psi(gamma, beta)> = prod_{l=1}^p [ e^{-i beta_l H_B} e^{-i gamma_l H_C} ] |+>^{tensor n}
    where H_B = sum_i X_i and H_C is the Ising problem Hamiltonian.
    """
    gammas = params[:p]
    betas = params[p:]

    qc = QuantumCircuit(n)
    # Initial state: equal superposition |+>^{tensor n}
    qc.h(range(n))

    for l in range(p):
        # Problem unitary: U(C, gamma) = exp(-i gamma H_C)
        # Implemented with RZZ(2 * gamma * J) for each edge
        for e in edges:
            qc.rzz(2.0 * gammas[l] * e["J"], e["u"], e["v"])

        # Mixer unitary: U(B, beta) = exp(-i beta H_B)
        # Implemented with RX(2 * beta) on each qubit
        for q in range(n):
            qc.rx(2.0 * betas[l], q)

    return qc


def compute_bitstring_cut(bitstring, edges):
    """Computes classical cut value for a binary string x in {0, 1}^n."""
    cut_val = 0.0
    for e in edges:
        u, v, w = e["u"], e["v"], 2.0 * e["J"]
        if bitstring[u] != bitstring[v]:
            cut_val += w
    return cut_val


def run_qaoa_experiment():
    json_candidates = [
        "B_discrete/results/maxcut_qubo.json",
        "results/maxcut_qubo.json",
        "../B_discrete/results/maxcut_qubo.json",
    ]
    data = load_maxcut_instance(json_candidates)

    n = data["num_nodes"]
    edges = data["ising_couplings"]
    opt_cut = float(data["optimal_cut"])
    cost_hamiltonian = build_cost_hamiltonian(data)

    csv_path = "C_quantum/results/qaoa_benchmark.csv"
    os.makedirs(os.path.dirname(csv_path), exist_ok=True)

    print("=========================================================================================")
    print("  Track C1: QAOA Max-Cut Optimization Benchmark (Qiskit Statevector Simulator)")
    print(f"  Graph: {n} Nodes, {len(edges)} Edges | Brute-force Global Optimum Cut = {opt_cut}")
    print("=========================================================================================\n")

    p_values = [1, 2, 3]
    num_seeds = 10
    base_seed = 100

    results_records = []

    print(f"{'p':<4} {'Seed':<6} {'Expected Cut':<14} {'Opt Cut':<10} {'Approx Ratio':<14} {'Best Cut':<10} {'P(Opt)':<10} {'Time (ms)':<10}")
    print("-" * 88)

    summary_stats = {}

    for p in p_values:
        ratios_p = []
        cuts_p = []
        p_opts_p = []
        runtimes_p = []

        for s_idx in range(num_seeds):
            seed = base_seed + s_idx
            np.random.seed(seed)

            # Heuristic initialization: gammas in [0, pi], betas in [0, pi/2]
            init_params = np.concatenate([
                np.random.uniform(0.0, np.pi, p),
                np.random.uniform(0.0, 0.5 * np.pi, p)
            ])

            def objective_func(params):
                qc = create_qaoa_circuit(n, edges, params, p)
                sv = Statevector(qc)
                # Minimize expected energy <H_C>
                return sv.expectation_value(cost_hamiltonian).real

            t_start = time.perf_counter()
            res = minimize(
                objective_func,
                init_params,
                method="COBYLA",
                options={"maxiter": 250, "tol": 1e-4}
            )
            t_end = time.perf_counter()
            runtime_ms = (t_end - t_start) * 1000.0

            # Evaluate final optimal statevector
            best_qc = create_qaoa_circuit(n, edges, res.x, p)
            final_sv = Statevector(best_qc)

            # Expected Cut = -<H_C>
            expected_cut = -final_sv.expectation_value(cost_hamiltonian).real
            approx_ratio = expected_cut / opt_cut

            # Measurement probability distribution over computational basis
            probs = final_sv.probabilities_dict()
            best_cut_sampled = 0.0
            p_opt = 0.0

            for bitstr_be, prob in probs.items():
                # Qiskit bitstrings are little-endian: reverse to match node indices 0..n-1
                bitstr = bitstr_be[::-1]
                cut = compute_bitstring_cut(bitstr, edges)
                if cut > best_cut_sampled:
                    best_cut_sampled = cut
                if abs(cut - opt_cut) < 1e-6:
                    p_opt += prob

            ratios_p.append(approx_ratio)
            cuts_p.append(expected_cut)
            p_opts_p.append(p_opt)
            runtimes_p.append(runtime_ms)

            results_records.append({
                "p": p,
                "seed": seed,
                "expected_cut": expected_cut,
                "optimal_cut": opt_cut,
                "approximation_ratio": approx_ratio,
                "best_sampled_cut": best_cut_sampled,
                "prob_optimal_state": p_opt,
                "runtime_ms": runtime_ms
            })

            print(f"{p:<4} {seed:<6} {expected_cut:<14.4f} {opt_cut:<10.1f} {approx_ratio:<14.4f} {best_cut_sampled:<10.1f} {p_opt:<10.4f} {runtime_ms:<10.2f}")

        mean_ratio = np.mean(ratios_p)
        std_ratio = np.std(ratios_p, ddof=1)
        mean_cut = np.mean(cuts_p)
        std_cut = np.std(cuts_p, ddof=1)
        mean_popt = np.mean(p_opts_p)
        std_popt = np.std(p_opts_p, ddof=1)
        mean_time = np.mean(runtimes_p)

        summary_stats[p] = {
            "mean_ratio": mean_ratio,
            "std_ratio": std_ratio,
            "mean_cut": mean_cut,
            "std_cut": std_cut,
            "mean_popt": mean_popt,
            "std_popt": std_popt,
            "mean_time_ms": mean_time
        }
        print("-" * 88)
        print(f"  -> Summary for p={p}: Approx Ratio = {mean_ratio:.4f} +/- {std_ratio:.4f} | "
              f"Expected Cut = {mean_cut:.2f} +/- {std_cut:.2f} | P(Opt) = {mean_popt:.3f} +/- {std_popt:.3f}\n")

    # Write detailed CSV
    with open(csv_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=[
            "p", "seed", "expected_cut", "optimal_cut", "approximation_ratio",
            "best_sampled_cut", "prob_optimal_state", "runtime_ms"
        ])
        writer.writeheader()
        writer.writerows(results_records)

    print(f"[SUCCESS] Benchmark completed. Detailed CSV exported to: {csv_path}\n")

    print("=========================================================================================")
    print("  QAOA Summary vs. Brute-Force Exact Solution (Optimal Cut = 15.0)")
    print("=========================================================================================")
    print(f"{'p (Depth)':<12} {'Approximation Ratio (Mean +/- Std)':<36} {'Expected Cut':<22} {'P(Opt State)':<20}")
    print("-" * 92)
    for p in p_values:
        s = summary_stats[p]
        ratio_str = f"{s['mean_ratio']:.4f} +/- {s['std_ratio']:.4f}"
        cut_str = f"{s['mean_cut']:.2f} +/- {s['std_cut']:.2f}"
        popt_str = f"{s['mean_popt']:.3f} +/- {s['std_popt']:.3f}"
        print(f"{p:<12} {ratio_str:<36} {cut_str:<22} {popt_str:<20}")
    print("=========================================================================================\n")


if __name__ == "__main__":
    run_qaoa_experiment()
