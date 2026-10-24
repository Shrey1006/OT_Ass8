# Prompt Log: I_quantum_algorithms - qiskit_algorithms_and_dwave_annealing
- **Timestamp**: 2026-10-24 16:53:35
- **Track**: I_quantum_algorithms
- **AI Tool & Model**: Antigravity (Gemini 3.8 Flash)

## Original Prompt
```text
I1. Implement Deutsch-Jozsa, Bernstein-Vazirani, Grover, QFT, QPE and Shor (small N) in Qiskit or PennyLane. For two of them, build the circuits manually from gates. Add a quantum annealing example (D-Wave simulator or simulated annealing, labelled clearly).
```

## AI Response (Summary / Relevant Excerpt)
```text
1. Implemented Deutsch-Jozsa (constant & balanced oracles).
2. Built Bernstein-Vazirani manually gate-by-gate with elementary H, X, CX gates.
3. Built Grover's Search manually gate-by-gate with custom phase oracle and diffusion operator using elementary H, X, CCX gates.
4. Implemented Quantum Fourier Transform (QFT) and verified exact unitarity (QFT^dag * QFT = I).
5. Implemented Quantum Phase Estimation (QPE) estimating theta = 3/8 with 100% precision.
6. Implemented Shor's algorithm for N=15 (a=7), discovering period r=4 and non-trivial factors 3 and 5.
7. Added Quantum Annealing example using D-Wave SimulatedAnnealingSampler (dwave-neal) on a 5-spin frustrated Ising model.
8. Integrated with C++ executable suite and 8 unit test assertions.
```

## Revised Prompt / User Iteration
```text
Initial implementation of Step I1.
```

## Commit Link
- Commit Hash / Tag: `[AI:Antigravity] Implement 6 quantum algorithms in Qiskit with manual gate circuits and D-Wave annealing simulation`

## Verification Note
1. Correctness Check: Verified all 6 quantum algorithms and D-Wave annealing simulation achieve 100% expected analytical and numerical results.
2. Baseline & Fair Comparison: Manual gate implementations decomposed strictly into elementary H, X, CX, and CCX gates.
3. Claims Verified: Verified Shor's algorithm successfully factors N=15 into 3 x 5 and Grover achieves >90% success probability.
