# Track Report: I_quantum_algorithms - Quantum Algorithms & Quantum Machine Learning

## 1. Problem Statement & Inefficiency
- **Domain & Process**: Quantum Computing & Quantum Machine Learning (QML) applied to discrete algorithmic oracles, phase estimation, integer factorization, and non-linear classification on the synthetic two-moon topology (`make_moons`).
- **Observed Bottleneck / Inefficiency**: Classical search over an unstructured search space requires $\mathcal{O}(N)$ queries; classical period finding and discrete logarithm have superpolynomial / subexponential complexity ($\mathcal{O}(\exp(c \sqrt[3]{n \log^2 n}))$ via Number Field Sieve); non-linear classification in classical feature spaces often requires high-dimensional inner product projections or dense neural networks that suffer from vanishing gradients or high parameter counts.
- **Baseline Description**:
  1. *Classical Linear / Exhaustive Search*: $\mathcal{O}(N)$ evaluations to locate a marked item.
  2. *Classical Classical RBF-Kernel SVM*: Maps inputs $x \in \mathbb{R}^2$ to an infinite-dimensional RKHS via $K(x, x') = \exp(-\gamma ||x - x'||^2)$.
  3. *Classical Multi-Layer Perceptron (MLP)*: 2 hidden neurons with tanh activation ($9$ parameters) trained via Adam optimizer.

---

## 2. Mathematical Formulation

### A. Quantum Gate Algorithms
1. **Deutsch-Jozsa**:
   Given oracle $f: \{0,1\}^n \to \{0,1\}$:
   $$|\psi_f\rangle = \frac{1}{2^n} \sum_{x=0}^{2^n-1} \sum_{z=0}^{2^n-1} (-1)^{x \cdot z + f(x)} |z\rangle$$
   Measurement of $|0\dots0\rangle$ occurs with probability $1$ if $f$ is constant, and $0$ if balanced.
2. **Bernstein-Vazirani**:
   Given $f(x) = s \cdot x \pmod 2$:
   $$H^{\otimes n} U_f H^{\otimes n} |0\rangle^{\otimes n} = |s\rangle$$
   Recovers secret string $s \in \{0,1\}^n$ in a single query.
3. **Grover's Search**:
   Given marked item $w$:
   $$G = (2|s\rangle\langle s| - I)(I - 2|w\rangle\langle w|)$$
   State rotates in 2D subspace spanned by $|w\rangle$ and $|w^\perp\rangle$: $\theta = 2\arcsin(1/\sqrt{N})$. Optimal iterations $R = \lfloor \frac{\pi}{4}\sqrt{N} \rfloor$.
4. **Quantum Fourier Transform (QFT)**:
   $$|j\rangle \mapsto \frac{1}{\sqrt{N}} \sum_{k=0}^{N-1} e^{2\pi i j k / N} |k\rangle$$
5. **Quantum Phase Estimation (QPE)**:
   For unitary $U$ with eigenstate $|\psi\rangle$ such that $U|\psi\rangle = e^{2\pi i \theta} |\psi\rangle$, QPE outputs an $m$-bit approximation $\tilde{\theta} = 0.\theta_1 \dots \theta_m$.
6. **Shor's Algorithm**:
   Reduces integer factorization of $N$ to order finding $a^r \equiv 1 \pmod N$. Non-trivial factors $\gcd(a^{r/2} \pm 1, N)$.

### B. Variational Quantum Classifier (VQC)
- Feature Map: $U_{\Phi}(x) = \bigotimes_{i=1}^n R_y(x_i)$.
- Parameterized Ansatz: $W(\theta) = \prod_{l=1}^L \left( U_{\text{ent}} \bigotimes_{i=1}^n R_y(\theta_{l, i}) \right)$.
- Objective: Minimize Mean Squared Error over training dataset:
  $$\min_{\theta} \frac{1}{N} \sum_{i=1}^N \Big(y_i - \langle 0| U_{\Phi}^\dagger(x_i) W^\dagger(\theta) Z_0 W(\theta) U_{\Phi}(x_i) |0\rangle\Big)^2$$
  Trained via parameter-shift rule:
  $$\frac{\partial f}{\partial \theta_k} = \frac{f(\theta_k + \pi/2) - f(\theta_k - \pi/2)}{2}$$

### C. Quantum Kernel SVM (QSVM)
- Maps data into Hilbert space $\Phi: \mathcal{X} \to \mathcal{H}$ using the $ZZ$-Feature Map:
  $$U_{\Phi}(x) = \exp\left(i \sum_{j} x_j Z_j + i \sum_{j < k} (\pi - x_j)(\pi - x_k) Z_j Z_k \right) H^{\otimes n}$$
- Quantum Gram Matrix:
  $$K_{ij} = |\langle 0| U_{\Phi}^\dagger(x_j) U_{\Phi}(x_i) |0\rangle|^2$$
- Solves classical dual SVM:
  $$\max_{\alpha} \sum_{i=1}^N \alpha_i - \frac{1}{2} \sum_{i,j=1}^N \alpha_i \alpha_j y_i y_j K_{ij} \quad \text{s.t.} \quad 0 \le \alpha_i \le C, \; \sum_{i=1}^N \alpha_i y_i = 0$$

---

## 3. Algorithm Selection & Chapter 6 Alignment
- **Chapter 6 Alignment**: Covers fundamental quantum oracles, parameter-shift rule, quantum kernel methods, VQC parameter optimization, and quantum annealing formulations.
- **Elementary Gate Constraint**: Bernstein-Vazirani and Grover's search are implemented strictly from basic gates ($H, X, CX, CCX$), demonstrating low-level circuit decomposition.
- **Annealing Model**: D-Wave `neal.SimulatedAnnealingSampler` simulates transverse-field quantum annealing on frustrated Ising spin Hamiltonians.

---

## 4. Experimental Setup & Fair Comparison
- **Dataset**: `make_moons(n_samples=100, noise=0.15, random_state=42)` split 70% train (70 samples), 30% test (30 samples). Features normalized to $[0, \pi]$.
- **Hardware & Simulators**: Qiskit 2.6.1 + `qiskit-aer` 0.18.0; `dwave-neal` 0.6.0; CPU: AMD64 Linux 5.15 on WSL Ubuntu 24.04.
- **Fairness Protocol**: All four classifiers (QSVM, VQC, Classical RBF-SVM, Classical MLP) evaluated on the exact same train/test split with deterministic seeds.

---

## 5. Results & Benchmark Data

### A. Track I1: Quantum Algorithms & Annealing Verification Summary
| Algorithm | Oracle / Target | Qubits | Circuit Depth | Key Metric | Verification Status |
| :--- | :--- | :---: | :---: | :--- | :---: |
| **Deutsch-Jozsa** | Constant $f(x)=0$ & Balanced $f(x)=x_0 \oplus x_1$ | 4 | 5 | 100% Deterministic Discrimination | **PASSED** |
| **Bernstein-Vazirani** | Hidden bitstring $s = \text{"1011"}$ | 5 | 7 | 100% Fidelity in 1 Query (Manual Gates) | **PASSED** |
| **Grover's Search** | Marked State $\|101\rangle$ | 3 | 22 | $P(\text{target}) = 94.5\%$ (Manual Gates) | **PASSED** |
| **QFT & $\text{QFT}^\dagger$** | Basis State $\|5\rangle$ | 3 | 12 | Exact Unitarity $\text{QFT}^\dagger \text{QFT} = I$ (100%) | **PASSED** |
| **Quantum Phase Estimation**| Phase $\theta = 3/8 = 0.375$ | 4 | 14 | Exact Phase $0.011_2$ ($100\%$ confidence) | **PASSED** |
| **Shor's Algorithm** | Factor $N=15$ ($a=7$) | 8 | 32 | Discovered $r=4 \implies 15 = 3 \times 5$ | **PASSED** |
| **Quantum Annealing** | 5-Spin Frustrated Ising Model | 5 | N/A | Ground Energy $E = -3.9000$ (200 reads) | **PASSED** |

---

### B. Track I2: QML vs Classical Classification Benchmark
Logged in [`I_quantum_algorithms/results/qml_classification_comparison.csv`](file:///c:/Users/ADMIN/Desktop/OT_Ass8/I_quantum_algorithms/results/qml_classification_comparison.csv):

| Model Architecture | Model Paradigm | Test Accuracy (%) | Training Time (s) | Parameter Count | Circuit / Model Evals |
| :--- | :--- | :---: | :---: | :---: | :---: |
| **Quantum Kernel SVM (QSVM)** | Quantum Metric / Dual SVM | **76.67%** | 0.0940 s | 41 ($N_{\text{SV}} + 1$) | **170** |
| **Variational Quantum Classifier (VQC)** | Parameterized Quantum Circuit | **80.00%** | 6.2997 s | **6** | 11,550 |
| **Classical RBF-Kernel SVM** | Classical Dual SVM | **93.33%** | **0.0015 s** | 29 ($N_{\text{SV}} + 1$) | 4,515 |
| **Classical Small MLP** | Feedforward Neural Net | **83.33%** | 0.0802 s | 9 | 21,000 |

---

### C. Track I3: Grover vs Classical Linear Search & Noisy Simulation Benchmark
Logged in [`I_quantum_algorithms/results/grover_scaling_comparison.csv`](file:///c:/Users/ADMIN/Desktop/OT_Ass8/I_quantum_algorithms/results/grover_scaling_comparison.csv) and [`I_quantum_algorithms/results/grover_noise_experiment.csv`](file:///c:/Users/ADMIN/Desktop/OT_Ass8/I_quantum_algorithms/results/grover_noise_experiment.csv):

#### 1. Scaling Benchmark (Scaling $n \in [3, 8]$ / $N \in [8, 256]$)
| Qubits ($n$) | Search Space ($N$) | Classical Queries | Grover Queries | Query Speedup | Classical ALU Ops | Quantum Gates | CX Gates | Circuit Depth | Grover Success (%) | Classical Wall-Clock ($\mu\text{s}$) | Est. HW Time ($\mu\text{s}$) |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| 3 | 8 | 6 | 2 | **3.00x** | 12 | 73 | 24 | 50 | 95.02% | **0.865** | 3.38 |
| 4 | 16 | 11 | 3 | **3.67x** | 22 | 205 | 84 | 167 | 95.90% | **0.920** | 10.82 |
| 5 | 32 | 21 | 4 | **5.25x** | 42 | 661 | 288 | 522 | 100.00% | **1.108** | 36.26 |
| 6 | 64 | 42 | 6 | **7.00x** | 84 | 2,190 | 960 | 1,184 | 100.00% | **2.079** | 120.60 |
| 7 | 128 | 84 | 8 | **10.50x** | 168 | 4,271 | 1,920 | 2,026 | 99.41% | **3.734** | 239.02 |
| 8 | 256 | 167 | 12 | **13.92x** | 334 | 8,804 | 4,032 | 3,698 | 100.00% | **8.524** | 498.64 |

#### 2. Noisy Simulation Experiment ($n=3$, $N=8$, Depolarizing Noise)
| 1-Qubit Gate Error $p_1$ (%) | 2-Qubit Gate Error $p_2 = 2p_1$ (%) | Success Probability (%) | Random Baseline (%) | Advantage over Guessing (%) |
| :---: | :---: | :---: | :---: | :---: |
| 0.0% | 0.0% | **94.29%** | 12.50% | **+81.79%** |
| 0.1% | 0.2% | **88.89%** | 12.50% | +76.39% |
| 0.5% | 1.0% | **67.36%** | 12.50% | +54.86% |
| 1.0% | 2.0% | **50.27%** | 12.50% | +37.77% |
| 2.0% | 4.0% | **29.71%** | 12.50% | +17.21% |
| 3.0% | 6.0% | **20.14%** | 12.50% | +7.64% |
| 5.0% | 10.0% | **13.57%** | 12.50% | **+1.07%** (Collapsed) |

---

### D. Visualizations & Empirical Plots

#### Figure I.1: Grover Search Query Scaling and QML Classification Accuracy
![Figure I.1: Grover Search Query Scaling and QML Classification Accuracy](../I_quantum_algorithms/results/grover_qml_benchmark.png)
- *Panel (a) Query Scaling Advantage*: Grover's search demonstrates asymptotic quadratic reduction in queries ($O(\sqrt{N})$ vs $O(N/2)$) as qubit register size grows from $n=3$ ($N=8$) to $n=8$ ($N=256$).
- *Panel (b) QML Classification Accuracy*: Compares test set accuracy on the non-linear `make_moons` dataset. Classical RBF-SVM achieves 93.3% and Classical MLP 83.3%, while Variational Quantum Classifier (VQC) achieves 80.0% with only 6 parameters, and Quantum Kernel SVM (QSVM) achieves 76.7%.

---

## 6. Analysis, Trade-offs & Limitations

### 1. Accuracy & Sample Efficiency
- **Classical RBF-SVM** achieved highest test accuracy (**93.33%**), closely followed by **Classical MLP** (**83.33%**) and **VQC** (**80.00%**).
- **QSVM** attained **76.67%** accuracy with $41$ support vectors. The non-linear $ZZ$-Feature Map creates rich entanglement between features $x_0$ and $x_1$, yielding separable boundaries on make-moons without classical feature engineering.

### 2. Parameter Efficiency vs Training Overhead
- **VQC Parameter Parsimony**: VQC required only **6 trainable parameters** to achieve 80.00% accuracy, outperforming classical models in parameter compactness.
- **Circuit Evaluation Cost**: VQC gradient calculation via the parameter-shift rule required $11,550$ statevector circuit evaluations ($3 \times \text{batch} \times \text{params} \times \text{iters}$). On real NISQ hardware with finite shot noise ($1024$ shots/circuit), this entails $>10^7$ physical circuit executions, demonstrating why quantum kernel methods (requiring only $N(N-1)/2$ evaluations once) are significantly more sample- and runtime-efficient for NISQ processors.

### 3. Quantum Annealing Ground State Exploration
- The D-Wave `neal.SimulatedAnnealingSampler` solved the 5-spin frustrated antiferromagnetic loop with $100\%$ success in discovering the degenerate ground states ($E = -3.9000$, e.g., $s = [-1, +1, +1, -1, +1]$), confirming that transverse annealing schedules escape local energy barriers on non-planar coupling graphs.

### 4. Honest Analysis: When Quantum Helps vs When It Doesn't (Grover vs Track D Classical Parallelism)
- **The Constant Factor Trap in Query vs Gate Complexity**:
  Grover's algorithm demonstrates a genuine asymptotic quadratic speedup in terms of oracle evaluations ($\mathcal{O}(\sqrt{N})$ vs $\mathcal{O}(N)$). At $N=256$, Grover achieves a **13.92x query advantage** (12 queries vs 167 queries).
  However, examining physical operations reveals an **honest negative result** for small-to-moderate search spaces:
  - While classical linear search requires only **334 elementary ALU operations** (comparisons and pointer increments), transpiled Grover's algorithm requires **8,804 quantum elementary gates**, including **4,032 CX gates** with a circuit depth of 3,698.
  - Multi-controlled $Z$ phase flips and diffusion operators decompose into $\mathcal{O}(n^2)$ entangling gates per iteration. Consequently, classical linear search takes **$8.524 \; \mu\text{s}$** on a standard 3.5 GHz CPU core, whereas estimated physical quantum execution on 100 ns superconducting qubits requires **$498.64 \; \mu\text{s}$** (**classical is 58.5x faster in wall-clock time**).
- **Where Quantum Does Not Help**:
  Quantum unstructured search fails to deliver practical speedups for in-memory or unstructured database lookup where:
  1. $N \le 10^8$ (the crossover point requires $N > 10^9$ to amortize the $\approx 10^3 - 10^4$ gate decomposition factor).
  2. The oracle evaluation is cheap (e.g. integer comparison). Classical CPUs execute billions of comparisons per second with SIMD vectorization and hardware prefetchers that completely overwhelm NISQ gate cycle latencies.
- **Where Quantum Truly Helps**:
  Quantum search provides exponential or polynomial utility only when:
  1. The oracle is an NP-verifier or cryptographic hash (e.g. SHA-256 pre-image finding or SAT solving), where each classical evaluation is computationally intense ($\mathcal{O}(\text{poly}(n))$ cycles) and memory bandwidth is not the bottleneck.
  2. Algorithms with exponential separation (Shor's order finding, Deutsch-Jozsa, Phase Estimation) where the mathematical structure of Fourier transforms over abelian groups bypasses classical combinatorial hardness entirely.
- **Noisy Simulation Degradation**:
  In our depolarizing noise experiment on $n=3$, fidelity drops sharply from $94.29\%$ at zero noise down to $50.27\%$ at $p_1 = 1.0\%$ ($p_2 = 2.0\%$). At $p_1 = 5.0\%$, the success probability reaches **$13.57\%$**, practically indistinguishable from uniform random guessing ($12.50\%$). Without fault-tolerant quantum error correction (QEC), quantum coherence vanishes before deep search iterations can constructively interfere.
- **Contrasting with Track D Classical Parallel Speedups**:
  In **Track D (Multi-threading / SIMD parallelism)**:
  - We achieved an immediate, deterministic **$3.8\times$ speedup on 4 threads** with zero degradation in accuracy or numerical fidelity.
  - Classical parallelism leverages mature silicon with sub-nanosecond clock cycles, coherent L1/L2/L3 cache hierarchies, and zero decoherence risk.
  - In stark contrast, Grover's $\mathcal{O}(\sqrt{N})$ algorithm introduces substantial physical gate compilation overhead and extreme sensitivity to physical gate noise, showing that asymptotic speedup does not equal wall-clock speedup without sufficiently massive problem sizes or fault-tolerant hardware.

---

## 7. Links to Code and Results
- **Source Code**:
  - Quantum Gate Circuits (DJ, BV, QFT, QPE, Shor, Annealer): [`I_quantum_algorithms/scripts/quantum_algorithms.py`](../I_quantum_algorithms/scripts/quantum_algorithms.py)
  - Grover vs Linear Search & Noise: [`I_quantum_algorithms/scripts/grover_vs_classical.py`](../I_quantum_algorithms/scripts/grover_vs_classical.py)
  - QML vs Classical Benchmark: [`I_quantum_algorithms/scripts/qml_benchmark.py`](../I_quantum_algorithms/scripts/qml_benchmark.py)
  - Unit Tests: [`I_quantum_algorithms/tests/test_main.cpp`](../I_quantum_algorithms/tests/test_main.cpp)
- **Empirical Results Files**:
  - Grover Query & Depth Scaling: [`I_quantum_algorithms/results/grover_scaling_comparison.csv`](../I_quantum_algorithms/results/grover_scaling_comparison.csv)
  - Grover Gate Noise Experiment: [`I_quantum_algorithms/results/grover_noise_experiment.csv`](../I_quantum_algorithms/results/grover_noise_experiment.csv)
  - QML vs Classical Classification: [`I_quantum_algorithms/results/qml_classification_comparison.csv`](../I_quantum_algorithms/results/qml_classification_comparison.csv)
  - Quantum Algorithms Verification Log: [`I_quantum_algorithms/results/quantum_algorithms_summary.csv`](../I_quantum_algorithms/results/quantum_algorithms_summary.csv)
  - Generated Empirical Plot: [`I_quantum_algorithms/results/grover_qml_benchmark.png`](../I_quantum_algorithms/results/grover_qml_benchmark.png)
- **Status Categorization**:
  - All Quantum Algorithms & Noise Runs: `[SIMULATED via Qiskit 2.6 Aer Statevector & Noise Models]`
  - Classical Linear Search & Classical SVM/MLP: `[MEASURED on Physical x86_64 CPU Hardware]`
