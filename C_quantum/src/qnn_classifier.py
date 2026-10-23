#!/usr/bin/env python3
"""
Track C3: Parameterized Quantum Circuit (PQC) / QNN Classifier
Implements and compares three quantum gradient computation methods:
  1) Parameter-Shift Rule (PSR)
  2) Simultaneous Perturbation Stochastic Approximation (SPSA)
  3) Quantum Natural Gradient (QNG via Fubini-Study metric tensor)
Evaluates convergence, accuracy, and circuit evaluation counts over 10 deterministic seeds.
"""

import os
import time
import csv
import numpy as np
from qiskit import QuantumCircuit
from qiskit.quantum_info import Statevector, SparsePauliOp


def generate_circle_dataset(n_samples=40, seed=42):
    """Generates a non-linear 2D concentric circles binary classification dataset."""
    np.random.seed(seed)
    n_half = n_samples // 2

    # Inner circle (class +1)
    r_in = np.random.uniform(0.1, 0.45, n_half)
    t_in = np.random.uniform(0.0, 2.0 * np.pi, n_half)
    X_in = np.stack([r_in * np.cos(t_in), r_in * np.sin(t_in)], axis=1)
    y_in = np.ones(n_half)

    # Outer ring (class -1)
    r_out = np.random.uniform(0.75, 1.25, n_half)
    t_out = np.random.uniform(0.0, 2.0 * np.pi, n_half)
    X_out = np.stack([r_out * np.cos(t_out), r_out * np.sin(t_out)], axis=1)
    y_out = -np.ones(n_half)

    X = np.concatenate([X_in, X_out], axis=0)
    y = np.concatenate([y_in, y_out], axis=0)
    return X, y


class PQCClassifier:
    """Parameterized Quantum Circuit Classifier with 2 qubits and 4 variational parameters."""
    def __init__(self, n_qubits=2, n_params=4):
        self.n_qubits = n_qubits
        self.n_params = n_params
        self.observable = SparsePauliOp.from_list([("IZ", 1.0)])

    def build_circuit(self, x, theta):
        qc = QuantumCircuit(self.n_qubits)
        # Feature encoding layer
        qc.ry(x[0] * np.pi, 0)
        qc.ry(x[1] * np.pi, 1)

        # Variational Layer 1
        qc.ry(theta[0], 0)
        qc.ry(theta[1], 1)
        qc.cx(0, 1)

        # Variational Layer 2
        qc.ry(theta[2], 0)
        qc.ry(theta[3], 1)
        return qc

    def predict_one(self, x, theta):
        qc = self.build_circuit(x, theta)
        sv = Statevector(qc)
        return sv.expectation_value(self.observable).real

    def predict(self, X, theta):
        return np.array([self.predict_one(x, theta) for x in X])

    def compute_loss_and_acc(self, X, y, theta):
        preds = self.predict(X, theta)
        loss = np.mean((preds - y) ** 2)
        acc = np.mean(np.sign(preds) == y) * 100.0
        return loss, acc, preds


def train_parameter_shift(pqc, X, y, init_theta, n_iters=35, lr=0.25):
    """Trains PQC using the exact analytic Parameter-Shift Rule."""
    theta = init_theta.copy()
    n_params = pqc.n_params
    n_samples = len(X)
    circuit_evals = 0

    loss_history = []
    acc_history = []

    for it in range(n_iters):
        loss_val, acc_val, preds = pqc.compute_loss_and_acc(X, y, theta)
        circuit_evals += n_samples
        loss_history.append(loss_val)
        acc_history.append(acc_val)

        grad = np.zeros(n_params)
        for k in range(n_params):
            tp, tm = theta.copy(), theta.copy()
            tp[k] += np.pi / 2.0
            tm[k] -= np.pi / 2.0

            preds_plus = np.array([pqc.predict_one(x, tp) for x in X])
            preds_minus = np.array([pqc.predict_one(x, tm) for x in X])
            circuit_evals += 2 * n_samples

            dy_dth = (preds_plus - preds_minus) / 2.0
            grad[k] = np.mean(2.0 * (preds - y) * dy_dth)

        theta -= lr * grad

    final_loss, final_acc, _ = pqc.compute_loss_and_acc(X, y, theta)
    circuit_evals += n_samples
    return {
        "final_loss": final_loss,
        "final_acc": final_acc,
        "circuit_evals": circuit_evals,
        "loss_history": loss_history,
        "acc_history": acc_history
    }


def train_spsa(pqc, X, y, init_theta, n_iters=35, lr=0.25, c0=0.15, gamma=0.101):
    """Trains PQC using Simultaneous Perturbation Stochastic Approximation (SPSA)."""
    theta = init_theta.copy()
    n_params = pqc.n_params
    n_samples = len(X)
    circuit_evals = 0

    loss_history = []
    acc_history = []

    for it in range(n_iters):
        loss_val, acc_val, preds = pqc.compute_loss_and_acc(X, y, theta)
        circuit_evals += n_samples
        loss_history.append(loss_val)
        acc_history.append(acc_val)

        # Bernoulli random perturbation Delta in {-1, +1}^D
        delta = 2.0 * np.random.randint(0, 2, n_params) - 1.0
        c_k = c0 / ((it + 1.0) ** gamma)

        theta_plus = theta + c_k * delta
        theta_minus = theta - c_k * delta

        preds_plus = pqc.predict(X, theta_plus)
        preds_minus = pqc.predict(X, theta_minus)
        circuit_evals += 2 * n_samples

        loss_plus = np.mean((preds_plus - y) ** 2)
        loss_minus = np.mean((preds_minus - y) ** 2)

        # Simultaneous perturbation gradient estimate
        grad_spsa = (loss_plus - loss_minus) / (2.0 * c_k * delta)
        theta -= lr * grad_spsa

    final_loss, final_acc, _ = pqc.compute_loss_and_acc(X, y, theta)
    circuit_evals += n_samples
    return {
        "final_loss": final_loss,
        "final_acc": final_acc,
        "circuit_evals": circuit_evals,
        "loss_history": loss_history,
        "acc_history": acc_history
    }


def train_qng(pqc, X, y, init_theta, n_iters=35, lr=0.25, eps=0.02):
    """Trains PQC using Quantum Natural Gradient preconditioned by the Fubini-Study metric tensor."""
    theta = init_theta.copy()
    n_params = pqc.n_params
    n_samples = len(X)
    circuit_evals = 0

    loss_history = []
    acc_history = []

    for it in range(n_iters):
        loss_val, acc_val, preds = pqc.compute_loss_and_acc(X, y, theta)
        circuit_evals += n_samples
        loss_history.append(loss_val)
        acc_history.append(acc_val)

        grad = np.zeros(n_params)
        F = np.zeros((n_params, n_params))

        # Compute empirical Fubini-Study tensor and parameter-shift gradient across data
        for i, x in enumerate(X):
            s0 = Statevector(pqc.build_circuit(x, theta)).data
            circuit_evals += 1
            dpsi = []

            for k in range(n_params):
                tp, tm = theta.copy(), theta.copy()
                tp[k] += np.pi / 2.0
                tm[k] -= np.pi / 2.0

                qc_p = pqc.build_circuit(x, tp)
                qc_m = pqc.build_circuit(x, tm)
                sp = Statevector(qc_p).data
                sm = Statevector(qc_m).data
                circuit_evals += 2

                dpsi.append((sp - sm) / 2.0)
                yp = Statevector(qc_p).expectation_value(pqc.observable).real
                ym = Statevector(qc_m).expectation_value(pqc.observable).real
                grad[k] += 2.0 * (preds[i] - y[i]) * (yp - ym) / (2.0 * n_samples)

            for k in range(n_params):
                for l in range(n_params):
                    term1 = np.vdot(dpsi[k], dpsi[l]).real
                    term2 = (np.vdot(dpsi[k], s0) * np.vdot(s0, dpsi[l])).real
                    F[k, l] += (term1 - term2) / n_samples

        # Regularized Quantum Fisher Information Matrix
        F_reg = F + eps * np.eye(n_params)
        nat_grad = np.linalg.solve(F_reg, grad)
        theta -= lr * nat_grad

    final_loss, final_acc, _ = pqc.compute_loss_and_acc(X, y, theta)
    circuit_evals += n_samples
    return {
        "final_loss": final_loss,
        "final_acc": final_acc,
        "circuit_evals": circuit_evals,
        "loss_history": loss_history,
        "acc_history": acc_history
    }


def run_benchmark():
    print("=========================================================================================")
    print("   Track C3: Quantum Gradient Benchmark (PSR vs. SPSA vs. QNG) on PQC Classifier")
    print("=========================================================================================\n")

    X, y = generate_circle_dataset(n_samples=40, seed=42)
    pqc = PQCClassifier(n_qubits=2, n_params=4)

    methods = [
        ("Parameter-Shift (PSR)", train_parameter_shift),
        ("SPSA", train_spsa),
        ("Quantum Natural Gradient (QNG)", train_qng),
    ]

    num_seeds = 10
    base_seed = 100
    n_iters = 35

    csv_path = "C_quantum/results/qnn_gradient_benchmark.csv"
    os.makedirs(os.path.dirname(csv_path), exist_ok=True)

    records = []
    summary_data = {}

    print(f"{'Method':<32} {'Seed':<6} {'Final Loss':<14} {'Final Acc (%)':<16} {'Circ Evals':<12} {'Time (ms)':<10}")
    print("-" * 92)

    for method_name, train_fn in methods:
        losses = []
        accs = []
        evals_list = []
        runtimes = []

        for s_idx in range(num_seeds):
            seed = base_seed + s_idx
            np.random.seed(seed)
            init_theta = np.random.uniform(-0.5, 0.5, pqc.n_params)

            t0 = time.perf_counter()
            res = train_fn(pqc, X, y, init_theta, n_iters=n_iters, lr=0.25)
            t1 = time.perf_counter()
            runtime_ms = (t1 - t0) * 1000.0

            losses.append(res["final_loss"])
            accs.append(res["final_acc"])
            evals_list.append(res["circuit_evals"])
            runtimes.append(runtime_ms)

            records.append({
                "method": method_name,
                "seed": seed,
                "final_loss": res["final_loss"],
                "final_acc": res["final_acc"],
                "circuit_evals": res["circuit_evals"],
                "runtime_ms": runtime_ms
            })

            print(f"{method_name:<32} {seed:<6} {res['final_loss']:<14.4f} {res['final_acc']:<16.1f} {res['circuit_evals']:<12} {runtime_ms:<10.1f}")

        summary_data[method_name] = {
            "mean_loss": np.mean(losses),
            "std_loss": np.std(losses, ddof=1),
            "mean_acc": np.mean(accs),
            "std_acc": np.std(accs, ddof=1),
            "mean_evals": np.mean(evals_list),
            "mean_time_ms": np.mean(runtimes)
        }
        print("-" * 92)
        s = summary_data[method_name]
        print(f"  -> {method_name} Summary: Loss = {s['mean_loss']:.4f} +/- {s['std_loss']:.4f} | "
              f"Acc = {s['mean_acc']:.1f}% +/- {s['std_acc']:.1f}% | Evals = {s['mean_evals']:.0f}\n")

    # Export to CSV
    with open(csv_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=[
            "method", "seed", "final_loss", "final_acc", "circuit_evals", "runtime_ms"
        ])
        writer.writeheader()
        writer.writerows(records)

    print(f"[SUCCESS] Benchmark complete. Results written to: {csv_path}\n")

    print("=========================================================================================")
    print("   Comprehensive Summary of Quantum Gradient Methods (10 Seeds per Optimizer)")
    print("=========================================================================================")
    print(f"{'Gradient Method':<32} {'Mean Final Loss':<20} {'Mean Accuracy (%)':<20} {'Circuit Evals':<16} {'Runtime (ms)':<12}")
    print("-" * 98)
    for method_name, s in summary_data.items():
        loss_str = f"{s['mean_loss']:.4f} +/- {s['std_loss']:.4f}"
        acc_str = f"{s['mean_acc']:.1f}% +/- {s['std_acc']:.1f}%"
        eval_str = f"{s['mean_evals']:.0f}"
        time_str = f"{s['mean_time_ms']:.1f}"
        print(f"{method_name:<32} {loss_str:<20} {acc_str:<20} {eval_str:<16} {time_str:<12}")
    print("=========================================================================================\n")


if __name__ == "__main__":
    run_benchmark()
