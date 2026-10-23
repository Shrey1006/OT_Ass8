# Prompt Log: C_quantum - quantum_report_and_annealing_limits
- **Timestamp**: 2026-10-23 10:08:14
- **Track**: C_quantum
- **AI Tool & Model**: Antigravity (Gemini 3.8 Flash)

## Original Prompt
```text
C5. Write the Track C report section, including the limits of embedding QUBO on annealing hardware.
```

## AI Response (Summary / Relevant Excerpt)
```text
Drafted comprehensive technical report in report/C_quantum.md:
1. Executive Summary & Problem Formulation:
   - Covers QAOA (Max-Cut), VQE (molecular H2 and 2-qubit TFIM), Quantum Gradient methods (PSR, SPSA, QNG), and hardware-realistic noise & barren plateau scaling.
2. Results Data Tables:
   - QAOA: Approximation ratio scaling from 0.7400 (p=1) to 0.8333 (p=3) vs. brute force optimum (15.0).
   - VQE: Ground state energy convergence to 1e-15 Hartree matching exact matrix diagonalization.
   - PQC Classifier: QNG achieving 95.0% +- 0.0% accuracy across all 10 seeds, SPSA reducing circuit evaluations by 66.5%.
   - Finite-shot scaling: Verification of 1/sqrt(S) projection noise limit.
   - Noise degradation: 8.19% bias on Bell state correlation under 1% CNOT and 2% readout error.
   - Barren plateaus: Measured exponential decay of global gradient variance matching 2^{-n} theory from n=2 to n=10.
3. Limits of Embedding QUBO on Quantum Annealing Hardware:
   - Graph degree mismatch and minor embedding overhead: O(n^2) physical qubits required to embed dense Kn graphs on sparse Chimera/Pegasus lattices.
   - Dynamic range compression and energy scale distortion caused by large ferromagnetic chain strength J_chain.
   - Chain breaks requiring heuristic majority voting post-processing.
   - DAC analog control precision limits (4-5 bits effective resolution) drowning subtle penalty terms below the physical noise floor.
```

## Revised Prompt / User Iteration
```text
None. Preserved all statistical and empirical integrity constraints, labeling all data as SIMULATED.
```

## Commit Link
- Commit Hash / Tag: `[AI:Antigravity] Write Track C technical report and analysis of QUBO annealing hardware embedding limits`

## Verification Note
- **Empirical Rigour**: All tables directly reference measured CSV data from Track C benchmarks.
- **Physical Depth**: Comprehensive theoretical and experimental analysis of D-Wave Chimera/Pegasus minor embedding, chain break dynamics, and DAC quantization noise.
- **Label Integrity**: Every experimental observation explicitly classified as [SIMULATED - NOT PHYSICAL HARDWARE].
