# Assignment 8: Optimize Anything (14-Day Vibe-Coding Challenge)
**Optimization Techniques | Lab Assignment 8**

Discover a process. Formulate the problem. Apply optimization. Prove the improvement.

---

## 🛠️ AI Tools Register
Per assignment requirements, all AI tools and model versions used across tracks are recorded below:

| Tool | Model Version | Tracks | Dates | Kind of Work |
| :--- | :--- | :--- | :--- | :--- |
| Antigravity | Gemini 3.8 Flash | Repo Setup, Tracks A-J | 2026-10-23 to 2026-11-06 | Planning, Code Generation, Debugging, Testing, Documentation |

---

## 📚 Chapter 6 Coverage Checklist
All methods explicitly covered in Chapter 6 are tracked below:

| Area | Algorithm / Technique | Experiment Description | Code Location | Result Summary |
| :--- | :--- | :--- | :--- | :--- |
| **ML Foundations** | ML as Optimization (Loss, Regularization) | Small linear/logistic regression setup | `A_continuous/` | Pending |
| **ML Foundations** | Continuous vs Discrete Optimization | Landscape comparison | `A_continuous/`, `B_discrete/` | Pending |
| **ML Foundations** | Convex vs Non-Convex (Local Min / Saddle) | 2D benchmark functions (e.g. Rastrigin/Saddle) | `A_continuous/` | Pending |
| **ML Foundations** | Basic Quantum Concepts in QML | Parameterized rotation / State encoding | `C_quantum/`, `I_quantum_algorithms/` | Pending |
| **Continuous** | Gradient Descent (GD) | Convex quadratic & logistic loss | `A_continuous/` | Pending |
| **Continuous** | SGD & Mini-batch SGD | Noisy gradient step comparison | `A_continuous/` | Pending |
| **Continuous** | Momentum & NAG (Nesterov) | Ill-conditioned quadratic ravine | `A_continuous/` | Pending |
| **Continuous** | AdaGrad, RMSProp, Adam | Sparse & non-convex optimization | `A_continuous/` | Pending |
| **Continuous** | Newton & Gauss-Newton | Exact Hessian & nonlinear least squares | `A_continuous/` | Pending |
| **Continuous** | Backpropagation | Gradient computation engine | `A_continuous/` | Pending |
| **Continuous** | Curvature Approximations (Fisher, HVPs) | Hessian-vector product & diagonal Fisher | `A_continuous/` | Pending |
| **Continuous** | Convergence (Convex vs Non-Convex) | Rate comparison across condition numbers | `A_continuous/` | Pending |
| **Continuous** | Step-size & Optimizer Tuning | Learning rate sweeps & schedule sensitivity | `A_continuous/` | Pending |
| **Continuous** | Noisy Objectives | Optimization under stochastic noise | `A_continuous/` | Pending |
| **Discrete** | Greedy Methods | Knapsack / Set cover heuristic | `B_discrete/` | Pending |
| **Discrete** | Integer Linear Programming (ILP) | Exact integer programming solver / Branching | `B_discrete/` | Pending |
| **Discrete** | Branch-and-Bound | Exact discrete search with bounding | `B_discrete/` | Pending |
| **Discrete** | Dynamic Programming | Optimal substructure allocation | `B_discrete/` | Pending |
| **Discrete** | Feature Selection / Combinatorial | Combinatorial subset selection | `B_discrete/` | Pending |
| **Discrete** | QUBO & Ising Formulation | Binary-to-spin mapping, penalty constraints | `B_discrete/` | Pending |
| **Quantum** | QAOA (Max-Cut) | Max-Cut on 3-regular graph with QAOA ansatz | `C_quantum/` | Pending |
| **Quantum** | VQE | Ground state energy calculation | `C_quantum/` | Pending |
| **Quantum** | PQC & QNN | Variational circuit classifier | `C_quantum/` | Pending |
| **Quantum** | Parameter-Shift Rule & SPSA | Analytical gradient vs stochastic gradient | `C_quantum/` | Pending |
| **Quantum** | Quantum Natural Gradient (QNG) | Fubini-Study metric tensor optimization | `C_quantum/` | Pending |
| **Quantum** | Hardware Noise & Shot Count Impact | Finite shot variance and depolarization noise | `C_quantum/` | Pending |
| **Quantum** | Barren Plateaus Study | Gradient variance scaling with qubit depth | `C_quantum/` | Pending |
| **Submodular** | Diminishing Returns & Submodularity Test | Marginal gain test on facility location / coverage | `D_submodular_parallel/` | Pending |
| **Submodular** | Greedy Submodular Optimization | $(1 - 1/e)$ approximation verification | `D_submodular_parallel/` | Pending |
| **Parallel** | Load Balancing & Work Stealing | Multi-threaded task scheduling comparison | `D_submodular_parallel/` | Pending |
| **Parallel** | Parallel Speedup & Amdahl/Gustafson | Scaling across 1..N cores / threads | `D_submodular_parallel/` | Pending |
| **Distributed** | FedAvg vs Centralized | Communication rounds vs local step epochs | `D_submodular_parallel/` | Pending |
| **Multi-Task** | MGDA & GradNorm vs Equal Weighting | Pareto multi-objective optimization | `D_submodular_parallel/` | Pending |
| **Case Study** | Main Real-Process Problem | End-to-end domain problem optimization | `E_case_study/` | Pending |
| **Compilers** | Constant Folding / DCE / CSE / Inlining | Pass pipeline optimization | `F_compiler/` | Pending |
| **Compilers** | Register Allocation / Instruction Schedule | Formulated as graph coloring / ILP | `F_compiler/` | Pending |
| **xv6 OS** | Kernel Scheduler / VM / Allocator | Priority / Stride scheduler or COW fork | `G_xv6/` | Pending |
| **DBMS** | Query Rewrite & Join Order Optimization | DP vs Greedy join enumeration | `H_dbms/` | Pending |
| **DBMS** | Index Selection under Storage Budget | Knapsack / ILP index selection | `H_dbms/` | Pending |
| **Quantum Algs**| Deutsch-Jozsa & Bernstein-Vazirani | Oracle algorithm implementations | `I_quantum_algorithms/` | Pending |
| **Quantum Algs**| Grover Search vs Classical Linear Search | Query count and runtime comparison | `I_quantum_algorithms/` | Pending |
| **Quantum Algs**| QFT & Phase Estimation | Fourier transform & eigenvalue estimation | `I_quantum_algorithms/` | Pending |
| **Quantum Algs**| Quantum Kernels vs Classical SVM | Kernel ridge / support vector classification | `I_quantum_algorithms/` | Pending |
| **SoftEng** | Software Engineering Task Optimization | Test suite minimization / build scheduling | `J_softeng/` | Pending |

---

## ⚙️ Setup Instructions Placeholder

### Prerequisites
- C++17 compliant compiler (`g++` >= 9.0, `clang++` >= 10.0, or MSVC 2019+)
- CMake >= 3.15
- Ninja or Make build system
- Git & Bash environment

### Building the Project
```bash
# Configure with CMake
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Build all tracks
cmake --build build

# Run all track tests and benchmarks
bash scripts/run_all.sh
```

---

## 📂 Repository Structure
```
.
├── A_continuous/           # Track A: Continuous optimization
├── B_discrete/             # Track B: Discrete & QUBO optimization
├── C_quantum/              # Track C: Quantum optimization (QAOA, VQE, etc.)
├── D_submodular_parallel/  # Track D: Submodular, parallel & distributed optimization
├── E_case_study/           # Track E: Main real-process case study
├── F_compiler/             # Track F: Compiler optimization
├── G_xv6/                  # Track G: xv6 OS optimization
├── H_dbms/                 # Track H: DBMS optimization
├── I_quantum_algorithms/   # Track I: Quantum algorithms & QML advantage
├── J_softeng/              # Track J: Software engineering optimization
├── prompts/                # Per-track prompt logs
├── report/                 # Technical reports per track
├── scripts/                # Utility & execution scripts
├── failure_log.md          # Documented AI failure cases and corrections
├── tool_comparison.md      # Head-to-head AI tool comparison
├── CMakeLists.txt          # Top-level build file
└── README.md
```
