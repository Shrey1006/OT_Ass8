# Assignment 8: Optimize Anything
## Final Comprehensive Technical Report

---

**Course**: Optimization Techniques  
**Assignment**: Assignment 8 — "Optimize Anything" (14-Day Vibe-Coding Challenge)  
**Student Name**: Shreyansh Mojidra  
**Roll Number**: 241070045  
**Submission Date**: October 28, 2026  
**Primary AI Pair**: Antigravity (Gemini 3.8 Flash)  
**Comparative AI Tool**: Claude 3.5 Sonnet  

---

## 1. Executive Overview

This comprehensive technical report documents the end-to-end mathematical formulation, empirical benchmarking, and rigorous comparative analysis of 44 distinct optimization algorithms and paradigms prescribed across Chapter 6 of the curriculum. Over an intensive 14-day cycle, ten specialized tracks spanning continuous optimization (gradient descent variants, adaptive step sizes, second-order Hessians, backpropagation), discrete combinatorial optimization (0/1 Knapsack, Max-Cut, QUBO, Ising formulations), quantum variational optimization (QAOA, VQE, QNN, noise models, barren plateaus), parallel and distributed systems (submodular coverage, work-stealing thread pools, FedAvg, multi-task gradient balancing), real-world systems engineering (Electric Vehicle VRPTW fleet routing), systems software (compiler IR passes, DSATUR register allocation, xv6 RISC-V proportional-share lottery scheduling, copy-on-write virtual memory), database engines (PostgreSQL TPC-H SF 0.1 join ordering and knapsack index selection), quantum algorithms (Grover search, Deutsch-Jozsa, Bernstein-Vazirani, QFT, QPE, Shor order finding, QML vs. classical SVM), and automated software engineering (pugixml test-suite 0-1 set cover minimization) were designed, implemented from first principles in C++17 and Python, and empirically verified against strict mathematical invariants. Every performance claim, speedup ratio, and trade-off documented herein is grounded strictly in concrete empirical measurements logged in repository CSV/JSON files, with simulated and theoretical models explicitly distinguished from physical hardware executions.

---

## 2. AI Tools Register

Per assignment requirements, all AI tools and model versions utilized during the planning, formulation, implementation, debugging, and verification phases are recorded below (copied verbatim from the repository registry):

| Tool | Model Version | Tracks | Dates | Kind of Work |
| :--- | :--- | :--- | :--- | :--- |
| **Antigravity** | Gemini 3.8 Flash | Repo Setup, Tracks A–J | 2026-10-23 to 2026-10-24 | Planning, Mathematical Formulation, C++17 Code Generation, Debugging, Test Harnesses, Benchmark Logging, Technical Reports |
| **Claude** (Comparison) | Claude 3.5 Sonnet | Tracks B, H | 2026-10-24 | Head-to-head algorithmic comparison on Knapsack / ILP branch-and-bound formulation |

---

## 3. Hardware & Software Evaluation Environment

All empirical benchmarks and execution measurements reported in this document were executed on physical hardware and host/subsystem environments characterized below:

### Physical Hardware Specifications
- **CPU**: Intel(R) Core(TM) i3-1005G1 CPU @ 1.20GHz (Base frequency 1.19 GHz, Turbo up to 3.40 GHz)
- **Physical Cores**: 2 physical execution cores
- **Logical Processors**: 4 hardware threads (2 threads per core via Intel Hyper-Threading)
- **Installed System Memory (RAM)**: 7.79 GB (~8.00 GB DDR4)
- **Storage Subsystem**: High-speed NVMe Solid-State Drive

### Operating Systems & Execution Subsystems
- **Host Operating System**: Microsoft Windows 11 Home Single Language (Version 10.0.26300, 64-bit AMD64 architecture)
- **Primary Linux Subsystem**: Windows Subsystem for Linux 2 (WSL2), running Linux Kernel `6.6.87.2-microsoft-standard-WSL2 #1 SMP PREEMPT_DYNAMIC Thu Jun 5 18:30:46 UTC 2025 x86_64`
- **Linux Distribution**: Ubuntu 24.04.1 LTS (Noble Numbat)

### Compilers, Toolchains & Runtimes
- **Primary C++ Compiler (WSL2 / Linux)**: `g++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0` (compiled with `-std=c++17 -O3 -fopenmp`)
- **Secondary Host C++ Compiler (Windows)**: `g++ (MinGW.org GCC-6.3.0-1) 6.3.0`
- **RISC-V Cross Compiler (xv6 OS Target)**: `riscv64-unknown-elf-gcc (Ubuntu 12.2.0-14ubuntu1)`
- **Build System**: CMake version 3.28.3 / GNU Make 4.4.1
- **Python Environment**: Python 3.12.3 (Linux x86_64)
- **Quantum & ML Libraries**: Qiskit 2.6.1, Qiskit Aer 0.18.0, SciPy 1.18.1, NumPy 1.26.4, Matplotlib 3.8.4, D-Wave Neal 0.6.0, Scikit-learn 1.4.2
- **JavaScript / Node Runtime**: Node.js v24.11.0 (Windows)
- **Virtual Machines & RDBMS**: QEMU RISC-V 64-bit emulator (`qemu-system-riscv64` version 8.2.2), PostgreSQL 16.15 relational database engine

---

## 4. Repository Architectural Map

The repository is modularly structured into 10 autonomous track directories, unified build definitions, automated verification test runners, prompt logging archives, and benchmark data catalogs:

```text
Assignment8_OptimizeAnything/
├── CMakeLists.txt              # Root CMake build file compiling all C++ tracks (A, B, D, E, F, G, H, I, J)
├── README.md                   # Comprehensive repository index, Chapter 6 checklist, setup steps & answers
├── failure_log.md              # Systematic engineering failure analysis (Newton, xv6, DBMS, SoftEng)
├── tool_comparison.md          # Head-to-head qualitative & quantitative study: Antigravity vs Claude
├── FINAL_TECHNICAL_REPORT.pdf  # Unified, publication-formatted final technical report PDF
├── A_continuous/               # Track A: First/second-order continuous optimization & backpropagation
│   ├── src/                    # C++17 implementations: optimizers, objectives, Hessians, MLP
│   ├── tests/                  # Unit test assertions (finite difference gradient verification)
│   ├── results/                # benchmark_summary.csv, convergence_trajectories.csv, PNG plots
│   └── scripts/                # Python trajectory plotting scripts
├── B_discrete/                 # Track B: Discrete combinatorial algorithms & QUBO / Ising mappings
│   ├── src/                    # Knapsack solvers (Greedy, DP, B&B, ILP), QUBO Max-Cut, mRMR
│   ├── tests/                  # Textbook invariant assertions (value=220, capacity=50)
│   └── results/                # knapsack_benchmark.csv, maxcut_qubo.json, knapsack_scaling.png
├── C_quantum/                  # Track C: Quantum variational algorithms (QAOA, VQE, QNN, noise models)
│   ├── src/                    # Qiskit/Aer implementations of QAOA, VQE, Parameter-Shift, QNG
│   └── results/                # qaoa_benchmark.csv, vqe_benchmark.csv, finite_shot, barren_plateaus
├── D_submodular_parallel/      # Track D: Submodular Max-Cover, parallel scaling, FedAvg, MGDA/GradNorm
│   ├── src/                    # Greedy coverage, OpenMP/std::thread work-stealing, FedAvg, MTL
│   ├── tests/                  # Submodularity inequality assertions (4,000 subset pairs verified)
│   └── results/                # parallel_scaling_benchmark.csv, fedavg_benchmark.csv, multitask_benchmark.csv
├── E_case_study/               # Track E: Main Real-World Case Study: Fleet Logistics (EV-VRPTW)
│   ├── src/                    # Solomon RC25 generator, Greedy NN baseline, SA-MNS, LNS ruin-and-recreate
│   ├── tests/                  # Route feasibility, capacity, time window, and SoC safety assertions
│   └── results/                # strategy_comparison.csv, baseline/LNS json solutions, route map plots
├── F_compiler/                 # Track F: Toy compiler TAC IR passes & graph-coloring register allocation
│   ├── src/                    # MiniLang TAC IR, Stack VM, 5 optimization passes, Kempe & DSATUR BnB
│   ├── tests/                  # Semantic equivalence harness asserting bitwise output identity on 15 programs
│   └── results/                # optimization_passes_benchmark.csv, register_allocation_benchmark.csv
├── G_xv6/                      # Track G: xv6 RISC-V lottery scheduler & copy-on-write (COW) fork
│   ├── xv6-riscv/              # Full operating system kernel tree, `settickets` syscall, `PTE_COW` trap handler
│   ├── tests/                  # Benchmark drivers (`bench_cpu`, `bench_io`, `bench_mixed`, `cowtest`)
│   └── results/                # xv6_lottery_cow_benchmark.png (scheduler ticks & COW latency/memory)
├── H_dbms/                     # Track H: PostgreSQL TPC-H SF 0.1 join ordering & knapsack index selection
│   ├── src/                    # Dynamic programming join order, 0-1 knapsack / ILP index selection
│   ├── scripts/                # Automated EXPLAIN ANALYZE executor, MD5 result checksum verifier
│   └── results/                # tpch_latency_comparison.csv, index_selection_comparison.csv, plots
├── I_quantum_algorithms/       # Track I: Quantum oracles, Grover vs linear search, QFT, QPE, Shor, QML
│   ├── src/ & scripts/         # Deutsch-Jozsa, Bernstein-Vazirani, Grover, QFT, QPE, QSVM, VQC
│   └── results/                # grover_scaling_comparison.csv, qml_classification_comparison.csv, plots
├── J_softeng/                  # Track J: Software engineering test-suite minimization on `pugixml`
│   ├── pugixml/                # Production C++ XML parser codebase (13,776 lines of code)
│   ├── src/                    # Test-to-subsystem incidence extractor, Greedy & Exact ILP set cover
│   ├── scripts/                # gprof profiling parser, mutation analysis harness
│   └── results/                # test_suite_minimization.csv, mutation_score_comparison.csv, plots
├── prompts/                    # Timestamped prompt archives, AI conversation logs, and verification notes
├── report/                     # Markdown technical chapters (00_intro, A through J, 99_conclusion)
└── scripts/                    # Unified verification runner (`run_all.sh`) and plot generators
```
