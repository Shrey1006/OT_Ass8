# Prompt Log: D_submodular_parallel - multitask_mgda_gradnorm_report
- **Timestamp**: 2026-10-23 17:15:05
- **Track**: D_submodular_parallel
- **AI Tool & Model**: Antigravity (Gemini 3.8 Flash)

## Original Prompt
```text
D4. Implement MGDA and GradNorm versus equal weighting on a two-task model. Write the Track D report section with honest conclusions.
```

## AI Response (Summary / Relevant Excerpt)
```text
Implemented Multi-Task Learning (MTL) balancing algorithms in D_submodular_parallel/src/multitask.hpp and authored report/D_submodular_parallel.md:
1. Multi-Task Model & Problem Setup:
   - Shared-bottom neural architecture (4D input, 4D shared hidden trunk with tanh activations, two conflicting task heads).
   - Tasks exhibit conflicting gradients with negative cosine similarity (mean cos(theta) ~ -0.36 to -0.66).
2. Algorithms Implemented:
   - Equal Weighting: Uniform loss aggregation (L_total = 0.5 L1 + 0.5 L2).
   - MGDA (Multiple-Gradient Descent Algorithm - Sener & Koltun 2018): Analytical minimum-norm vector in convex hull of gradients.
   - GradNorm (Chen et al. 2018): Dynamic loss weighting based on relative inverse training rates r_i^alpha.
3. Empirical Results (10 deterministic seeds, 100 epochs):
   - Equal Weighting: Task 1 Loss = 0.1866 +- 0.0354 | Task 2 Loss = 0.3271 +- 0.1189 | Total = 0.5144 +- 0.1388
   - MGDA: Task 1 Loss = 0.1879 +- 0.0632 | Task 2 Loss = 0.4994 +- 0.3006 | Total = 0.6873 +- 0.2889
   - GradNorm: Task 1 Loss = 0.1590 +- 0.0267 | Task 2 Loss = 0.3069 +- 0.0768 | Total = 0.4659 +- 0.0911 (Best overall!)
4. Honest Conclusions & Technical Report:
   - Authored report/D_submodular_parallel.md covering all four Track D topics:
     * Submodular max-coverage & (1 - 1/e) guarantee.
     * Parallel workload scaling, Amdahl/Gustafson predictions, and granularity breakdown.
     * FedAvg vs. Centralized SGD and non-IID client drift.
     * Multi-task optimization analysis explaining why MGDA suffers from premature training stall on conflicting objectives.
```

## Revised Prompt / User Iteration
```text
None. Preserved all statistical and empirical integrity constraints, documenting honest limitations.
```

## Commit Link
- Commit Hash / Tag: `[AI:Antigravity] Implement MGDA and GradNorm vs Equal Weighting and write Track D technical report`

## Verification Note
- **Empirical Rigour**: All tables directly reference measured CSV data from Track D benchmarks.
- **Pareto Dynamics**: Explained the premature convergence trap of MGDA when conflicting gradients cause the convex hull minimum norm to approach zero.
- **Reporting Completeness**: Formally drafted report/D_submodular_parallel.md with full mathematical derivations and honest limitations.
