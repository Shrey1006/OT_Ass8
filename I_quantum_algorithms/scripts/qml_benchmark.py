#!/usr/bin/env python3
"""
Track I2: Variational Quantum Classifier (VQC) and Quantum Kernel SVM (QSVM)
versus Equivalent Classical Models (RBF-SVM, Small Classical MLP) on the Same Dataset.

Metrics Reported:
  1. Test Accuracy (%)
  2. Training Time (seconds)
  3. Parameter Count
  4. Quantum Circuit / Model Evaluations
"""

import time
import os
import sys
import numpy as np
from sklearn.datasets import make_moons
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import MinMaxScaler
from sklearn.svm import SVC
from sklearn.neural_network import MLPClassifier

# Qiskit 2.x
from qiskit import QuantumCircuit, transpile
from qiskit.circuit import Parameter, ParameterVector
from qiskit_aer import AerSimulator
from qiskit.quantum_info import Statevector

# Set seeds for strict reproducibility
np.random.seed(42)

# ==============================================================================
# 1. DATASET GENERATION (Non-linear Make-Moons)
# ==============================================================================
def get_dataset(n_samples=100, noise=0.15, test_size=0.30):
    X, y = make_moons(n_samples=n_samples, noise=noise, random_state=42)
    scaler = MinMaxScaler(feature_range=(0.0, np.pi))
    X_scaled = scaler.fit_transform(X)
    X_train, X_test, y_train, y_test = train_test_split(
        X_scaled, y, test_size=test_size, random_state=42, stratify=y
    )
    return X_train, X_test, y_train, y_test

# ==============================================================================
# 2. QUANTUM FEATURE MAP & KERNEL (QSVM)
# ==============================================================================
def create_zz_feature_map(n_qubits=2):
    """
    Standard 2-qubit ZZ Feature Map:
    H^n -> Rz(2*x_i) -> Entangling ZZ interaction Rz(2*(pi-x0)*(pi-x1))
    """
    x = ParameterVector("x", n_qubits)
    qc = QuantumCircuit(n_qubits, name="ZZFeatureMap")
    for i in range(n_qubits):
        qc.h(i)
        qc.rz(2.0 * x[i], i)
    qc.cx(0, 1)
    qc.rz(2.0 * (np.pi - x[0]) * (np.pi - x[1]), 1)
    qc.cx(0, 1)
    return qc, x

def compute_quantum_kernel_matrix(X1, X2, feature_map, x_params, is_symmetric=False):
    """
    Computes Gram matrix K_ij = |<Phi(x_i)|Phi(x_j)>|^2
    using exact Statevector fidelity for clean, deterministic evaluation.
    Tracks exact number of circuit evaluations.
    """
    n1 = len(X1)
    n2 = len(X2)
    K = np.zeros((n1, n2))
    circuit_evals = 0

    # Precompute statevectors for X1
    sv_1 = []
    for i in range(n1):
        bound_circ = feature_map.assign_parameters({x_params: X1[i]})
        sv_1.append(Statevector.from_instruction(bound_circ))
        circuit_evals += 1

    if is_symmetric:
        sv_2 = sv_1
    else:
        sv_2 = []
        for j in range(n2):
            bound_circ = feature_map.assign_parameters({x_params: X2[j]})
            sv_2.append(Statevector.from_instruction(bound_circ))
            circuit_evals += 1

    for i in range(n1):
        start_j = (i if is_symmetric else 0)
        for j in range(start_j, n2):
            # Fidelity |<psi_i | psi_j>|^2
            overlap = sv_1[i].inner(sv_2[j])
            fidelity = np.abs(overlap) ** 2
            K[i, j] = fidelity
            if is_symmetric:
                K[j, i] = fidelity

    return K, circuit_evals

def train_qsvm(X_train, y_train, X_test, y_test):
    print(" -> Training Model: Quantum Kernel SVM (QSVM)...")
    t0 = time.time()
    feature_map, x_params = create_zz_feature_map(n_qubits=2)

    # 1. Train Gram matrix
    K_train, evals_train = compute_quantum_kernel_matrix(
        X_train, X_train, feature_map, x_params, is_symmetric=True
    )

    # 2. Test Gram matrix
    K_test, evals_test = compute_quantum_kernel_matrix(
        X_test, X_train, feature_map, x_params, is_symmetric=False
    )

    total_circuit_evals = evals_train + evals_test

    # 3. Fit classical dual SVM with precomputed quantum kernel
    clf = SVC(kernel="precomputed", C=1.0)
    clf.fit(K_train, y_train)

    train_time = time.time() - t0
    y_pred = clf.predict(K_test)
    acc = np.mean(y_pred == y_test) * 100.0

    # Parameter count = support vectors dual coefficients + bias
    n_params = len(clf.support_) + 1

    return {
        "model": "Quantum Kernel SVM (QSVM)",
        "accuracy": round(acc, 2),
        "train_time_sec": round(train_time, 4),
        "parameter_count": n_params,
        "circuit_evaluations": total_circuit_evals,
        "support_vectors": len(clf.support_)
    }

# ==============================================================================
# 3. VARIATIONAL QUANTUM CLASSIFIER (VQC / PQC)
# ==============================================================================
def create_vqc_circuit(n_qubits=2, n_layers=2):
    """
    Constructs parameterized VQC:
    Feature Map: Angle encoding (Ry(x_i))
    Ansatz: L layers of Ry(theta) and entangling CX gates
    """
    x = ParameterVector("x", n_qubits)
    theta = ParameterVector("theta", (n_layers + 1) * n_qubits)
    qc = QuantumCircuit(n_qubits, name="VQC")

    # Feature encoding
    for i in range(n_qubits):
        qc.ry(x[i], i)

    # Variational layers
    p_idx = 0
    for l in range(n_layers):
        for i in range(n_qubits):
            qc.ry(theta[p_idx], i)
            p_idx += 1
        qc.cx(0, 1)

    for i in range(n_qubits):
        qc.ry(theta[p_idx], i)
        p_idx += 1

    return qc, x, theta

def evaluate_vqc_state(qc, x_params, theta_params, x_val, theta_val):
    """
    Evaluates expectation value <Z_0> using Statevector simulation.
    """
    bind_dict = {x_params: x_val, theta_params: theta_val}
    bound = qc.assign_parameters(bind_dict)
    sv = Statevector.from_instruction(bound)
    # Expectation of Z on qubit 0: sum_i (-1)^(b_0) |c_i|^2
    # In 2-qubit computational basis: |00>=+1, |01>=+1, |10>=-1, |11>=-1
    probs = sv.probabilities()
    exp_z0 = (probs[0] + probs[1]) - (probs[2] + probs[3])
    return exp_z0

def train_vqc(X_train, y_train, X_test, y_test, n_layers=2, max_iters=50, lr=0.15):
    print(" -> Training Model: Variational Quantum Classifier (VQC)...")
    t0 = time.time()
    n_qubits = 2
    qc, x_params, theta_params = create_vqc_circuit(n_qubits=n_qubits, n_layers=n_layers)
    n_params = len(theta_params)

    # Initial random weights
    theta = np.random.uniform(-np.pi / 2, np.pi / 2, n_params)
    y_train_signed = 2 * y_train - 1  # Map {0, 1} to {-1, +1}
    y_test_signed = 2 * y_test - 1

    circuit_evals = 0
    shift = np.pi / 2.0  # Parameter shift rule

    # Training loop with parameter-shift rule
    for epoch in range(max_iters):
        # Sample mini-batch of 16 points for speed
        batch_idx = np.random.choice(len(X_train), size=min(16, len(X_train)), replace=False)
        grad = np.zeros(n_params)

        for p in range(n_params):
            theta_plus = np.copy(theta)
            theta_plus[p] += shift

            theta_minus = np.copy(theta)
            theta_minus[p] -= shift

            # Parameter shift gradient over batch
            d_loss = 0.0
            for idx in batch_idx:
                x_val = X_train[idx]
                y_val = y_train_signed[idx]

                y_pred_center = evaluate_vqc_state(qc, x_params, theta_params, x_val, theta)
                y_pred_plus   = evaluate_vqc_state(qc, x_params, theta_params, x_val, theta_plus)
                y_pred_minus  = evaluate_vqc_state(qc, x_params, theta_params, x_val, theta_minus)
                circuit_evals += 3

                # d/d_theta f = (f(theta + pi/2) - f(theta - pi/2)) / 2
                d_pred = 0.5 * (y_pred_plus - y_pred_minus)
                # MSE loss: (y_pred - y_true)^2 -> grad = 2 * (y_pred - y_true) * d_pred
                d_loss += 2.0 * (y_pred_center - y_val) * d_pred

            grad[p] = d_loss / len(batch_idx)

        # Gradient descent step
        theta -= lr * grad

    train_time = time.time() - t0

    # Test evaluation
    correct = 0
    for i in range(len(X_test)):
        pred = evaluate_vqc_state(qc, x_params, theta_params, X_test[i], theta)
        circuit_evals += 1
        predicted_class = 1 if pred >= 0.0 else 0
        if predicted_class == y_test[i]:
            correct += 1

    acc = (correct / float(len(X_test))) * 100.0

    return {
        "model": "Variational Quantum Classifier (VQC)",
        "accuracy": round(acc, 2),
        "train_time_sec": round(train_time, 4),
        "parameter_count": n_params,
        "circuit_evaluations": circuit_evals,
        "support_vectors": "N/A"
    }

# ==============================================================================
# 4. EQUIVALENT CLASSICAL MODELS (RBF-SVM & Classical Small MLP)
# ==============================================================================
def train_classical_rbf_svm(X_train, y_train, X_test, y_test):
    print(" -> Training Model: Classical RBF-Kernel SVM (RBF-SVM)...")
    t0 = time.time()
    clf = SVC(kernel="rbf", C=1.0, gamma="scale")
    clf.fit(X_train, y_train)
    train_time = time.time() - t0

    y_pred = clf.predict(X_test)
    acc = np.mean(y_pred == y_test) * 100.0
    n_params = len(clf.support_) + 1

    # Classical kernel evaluations: N_train * N_train / 2 + N_test * N_train
    evals = (len(X_train) * (len(X_train) - 1)) // 2 + len(X_test) * len(X_train)

    return {
        "model": "Classical RBF-Kernel SVM",
        "accuracy": round(acc, 2),
        "train_time_sec": round(train_time, 4),
        "parameter_count": n_params,
        "circuit_evaluations": evals,
        "support_vectors": len(clf.support_)
    }

def train_classical_mlp(X_train, y_train, X_test, y_test):
    print(" -> Training Model: Classical Small MLP (2 Hidden Neurons)...")
    t0 = time.time()
    # 2 inputs -> 2 hidden neurons with bias -> 1 output with bias
    # Total params = (2*2 + 2) + (2*1 + 1) = 6 + 3 = 9 parameters!
    mlp = MLPClassifier(
        hidden_layer_sizes=(2,),
        activation="tanh",
        solver="adam",
        max_iter=500,
        random_state=42
    )
    mlp.fit(X_train, y_train)
    train_time = time.time() - t0

    y_pred = mlp.predict(X_test)
    acc = np.mean(y_pred == y_test) * 100.0

    # Count parameters: weights + biases
    n_params = sum(coef.size for coef in mlp.coefs_) + sum(intercept.size for intercept in mlp.intercepts_)
    evals = mlp.n_iter_ * len(X_train)

    return {
        "model": "Classical Small MLP (9 Params)",
        "accuracy": round(acc, 2),
        "train_time_sec": round(train_time, 4),
        "parameter_count": n_params,
        "circuit_evaluations": evals,
        "support_vectors": "N/A"
    }

# ==============================================================================
# MAIN BENCHMARK DRIVER
# ==============================================================================
def main():
    print("=========================================================================")
    print(" Track I2: QML vs Classical Classification Benchmark                    ")
    print(" Dataset: Make-Moons (N=100, Noise=0.15, Train=70, Test=30)             ")
    print("=========================================================================\n")

    X_train, X_test, y_train, y_test = get_dataset()

    res_qsvm   = train_qsvm(X_train, y_train, X_test, y_test)
    res_vqc    = train_vqc(X_train, y_train, X_test, y_test, max_iters=40)
    res_rbf    = train_classical_rbf_svm(X_train, y_train, X_test, y_test)
    res_mlp    = train_classical_mlp(X_train, y_train, X_test, y_test)

    results = [res_qsvm, res_vqc, res_rbf, res_mlp]

    print("\n---------------------------------------------------------------------------------------------------------")
    print(f"{'Model Architecture':<36} {'Test Acc (%)':<14} {'Train Time (s)':<16} {'Params':<10} {'Circuit/Func Evals':<20}")
    print("---------------------------------------------------------------------------------------------------------")
    for r in results:
        print(f"{r['model']:<36} {r['accuracy']:<14.2f} {r['train_time_sec']:<16.4f} {str(r['parameter_count']):<10} {str(r['circuit_evaluations']):<20}")
    print("---------------------------------------------------------------------------------------------------------")

    # Export CSV
    out_dir = os.path.join(os.path.dirname(__file__), "..", "results")
    os.makedirs(out_dir, exist_ok=True)
    out_csv = os.path.join(out_dir, "qml_classification_comparison.csv")
    with open(out_csv, "w") as f:
        f.write("model,accuracy_pct,training_time_sec,parameter_count,circuit_or_model_evaluations\n")
        for r in results:
            f.write(f"\"{r['model']}\",{r['accuracy']},{r['train_time_sec']},{r['parameter_count']},{r['circuit_evaluations']}\n")
    print(f"\n[INFO] Results saved to: {out_csv}")

    print("\n=========================================================================")
    print(" >>> Track I2 QML vs Classical Benchmark Completed Successfully! <<<     ")
    print("=========================================================================")

if __name__ == "__main__":
    main()
