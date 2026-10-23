# Prompt Log: C_quantum - qaoa_maxcut_benchmark
- **Timestamp**: 2026-10-23 09:34:09
- **Track**: C_quantum
- **AI Tool & Model**: Antigravity (Gemini 3.8 Flash)

## Original Prompt
```text
C1. Using Qiskit or PennyLane (Python, simulator), solve the Max-Cut QUBO from Track B (load the JSON) with QAOA at p=1,2,3. Compare with the brute-force optimum and report approximation ratio over 10 seeds.
```

## AI Response (Summary / Relevant Excerpt)
```text
Implemented Qiskit QAOA simulation in C_quantum/src/qaoa_maxcut.py:
1. Model & Data Ingestion:
   - Loaded 6-node, 10-edge Erdős-Rényi Max-Cut problem graph and Ising parameters from B_discrete/results/maxcut_qubo.json.
   - Brute-force global optimum cut value = 15.0.
   - Built problem Hamiltonian H_C = sum J_{uv} Z_u Z_v + offset * I and mixer H_B = sum X_i.
2. Variational QAOA Optimization:
   - Parameterized ansatz with p in {1, 2, 3} layers using RZZ(2 * gamma * J) and RX(2 * beta) rotations.
   - Simulated exact statevector expectation values using Qiskit Statevector simulator.
   - Optimized classical angles using SciPy COBYLA over 10 deterministic seeds (100..109) per depth p.
3. Results & Convergence:
   - p = 1: Approximation Ratio = 0.7400 +- 0.0630 | Expected Cut = 11.10 +- 0.95 | P(Opt) = 0.033 +- 0.041
   - p = 2: Approximation Ratio = 0.7860 +- 0.0592 | Expected Cut = 11.79 +- 0.89 | P(Opt) = 0.102 +- 0.076
   - p = 3: Approximation Ratio = 0.8333 +- 0.0447 | Expected Cut = 12.50 +- 0.67 | P(Opt) = 0.131 +- 0.062
   - In 100% of runs, the best sampled basis state achieved the global optimum cut of 15.0.
4. Export:
   - Detailed CSV generated at C_quantum/results/qaoa_benchmark.csv.
```

## Revised Prompt / User Iteration
```text
None. Successfully configured Qiskit 2.6 and SciPy in WSL and directly executed QAOA against Track B Max-Cut JSON.
```

## Commit Link
- Commit Hash / Tag: `[AI:Antigravity] Solve Track B Max-Cut with QAOA at p=1,2,3 across 10 seeds using Qiskit`

## Verification Note
- **Deterministic Check**: 10 deterministic seeds evaluated per depth, reporting Mean ± Std for approximation ratio and expected cut.
- **Circuit Depth Scaling**: Demonstrated monotonic improvement in mean approximation ratio ($0.740 \to 0.786 \to 0.833$) as depth $p$ increases from 1 to 3.
- **Ground State Concentration**: Probability of measuring the exact optimal ground state increased ~4x from $p=1$ (3.3%) to $p=3$ (13.1%).
