# Prompt Log: C_quantum - qnn_gradient_comparison
- **Timestamp**: 2026-10-23 09:54:21
- **Track**: C_quantum
- **AI Tool & Model**: Antigravity (Gemini 3.8 Flash)

## Original Prompt
```text
C3. Implement gradient computation by parameter-shift, SPSA and quantum natural gradient for a parameterized circuit; compare convergence and number of circuit evaluations. Build a small QNN / PQC classifier.
```

## AI Response (Summary / Relevant Excerpt)
```text
Implemented Parameterized Quantum Circuit (PQC) classifier and 3 quantum gradient optimization strategies in C_quantum/src/qnn_classifier.py:
1. PQC Classifier Architecture:
   - 2-qubit ansatz with angle feature map (RY(x0 * pi), RY(x1 * pi)) and 4 variational parameters.
   - Non-linear 2D concentric circles binary classification dataset (40 samples).
   - Observable Z0 measuring expectation <Z0> in [-1, +1] with MSE loss.
2. Gradient Computation Methods:
   - Parameter-Shift Rule (PSR): Exact analytic gradient via [f(theta + pi/2) - f(theta - pi/2)] / 2.
     Evaluations per iteration: N * (1 + 2D) = 360 circuits.
   - SPSA (Simultaneous Perturbation Stochastic Approximation): Random Bernoulli vector +/- delta.
     Evaluations per iteration: 2N = 80 circuits (constant in parameter dimension D).
   - Quantum Natural Gradient (QNG): Preconditioned by the empirical Fubini-Study metric tensor F_ij = Re[<d_i psi|d_j psi> - <d_i psi|psi><psi|d_j psi>].
     Evaluations per iteration: 400 circuits.
3. Statistical Evaluation (10 deterministic seeds, 35 iterations):
   - Parameter-Shift (PSR):
     * Final Loss: 0.4603 +- 0.0880
     * Accuracy: 86.5% +- 3.4%
     * Circuit Evaluations: 12,640
     * Runtime: 9,391 ms
   - SPSA:
     * Final Loss: 0.4638 +- 0.1340
     * Accuracy: 86.8% +- 6.2%
     * Circuit Evaluations: 4,240 (66.5% evaluation reduction)
     * Runtime: 3,883 ms
   - Quantum Natural Gradient (QNG):
     * Final Loss: 0.3311 +- 0.0001 (lowest loss, near-zero variance)
     * Accuracy: 95.0% +- 0.0% (100% of seeds reached 95% accuracy)
     * Circuit Evaluations: 14,040
     * Runtime: 20,560 ms
4. Export:
   - Detailed per-seed logs saved to C_quantum/results/qnn_gradient_benchmark.csv.
```

## Revised Prompt / User Iteration
```text
None. Formulated exact Fubini-Study tensor for QNG, parameter shift rule for PSR, and simultaneous perturbation for SPSA as specified.
```

## Commit Link
- Commit Hash / Tag: `[AI:Antigravity] Implement QNN classifier comparing Parameter-Shift, SPSA, and Quantum Natural Gradient`

## Verification Note
- **Algorithmic Evaluation Parity**: Evaluated on identical 2D concentric circles dataset across 10 deterministic seeds (100..109).
- **Circuit Evaluation Trade-offs**: SPSA required 66.5% fewer circuit evaluations (4,240 vs. 12,640 for PSR) while achieving comparable mean accuracy ($86.8\%$ vs. $86.5\%$).
- **Geometric Invariance of QNG**: Preconditioning with the Fubini-Study metric tensor produced superior convergence ($95.0\% \pm 0.0\%$ accuracy across all 10 seeds with loss $0.3311$).
