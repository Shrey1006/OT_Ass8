# Prompt Log: B_discrete - B2_maxcut_qubo_ising
- **Timestamp**: 2026-10-23 13:05:37
- **Track**: B_discrete
- **AI Tool & Model**: Antigravity (Gemini 3.8 Flash)

## Original Prompt
```text
B2. Add Max-Cut on random graphs: greedy, exhaustive search for small n, and a QUBO/Ising formulation with explicit binary-to-spin conversion (x = (1+s)/2). Add a penalty-term example by encoding the knapsack constraint as a QUBO. Solve the QUBO classically with simulated annealing and brute force, and export the Max-Cut QUBO to a JSON file for Track C.
```

## AI Response (Summary / Relevant Excerpt)
```text
Implemented QUBO, Ising conversion, penalty terms, and Max-Cut graph algorithms in B_discrete/src/qubo.hpp and B_discrete/src/maxcut.hpp:
1. opt::QUBO: Matrix representation for quadratic unconstrained binary optimization.
2. Explicit Binary-to-Spin Conversion: Uses x_i = (1 + s_i) / 2 to analytically map QUBO matrices to Ising Hamiltonians H(s) = sum_{i<j} J_ij * s_i * s_j + sum_i h_i * s_i + offset.
3. Penalty-term encoding: Implemented QUBO::from_knapsack with quadratic penalty P * (sum w_i x_i + sum 2^k y_k - W)^2 using binary slack variables.
4. Classical Solvers: Exhaustive brute force enumeration and Simulated Annealing (Metropolis-Hastings).
5. opt::MaxCutGraph: Erdos-Renyi random graph generator, greedy partition heuristic, exhaustive exact solver, and automatic to_qubo() / to_ising() translation.
6. Exported Max-Cut QUBO and ground truth solutions to B_discrete/results/maxcut_qubo.json for Track C.
7. Unit tests in B_discrete/tests/test_main.cpp verifying exact E_QUBO(x) == E_Ising(s) equivalence, knapsack penalty recovery of exact optimum (val 220, wt 50), and 10-seed Simulated Annealing Max-Cut results.
```

## Revised Prompt / User Iteration
```text
N/A (First-pass implementation)
```

## Commit Link
- Commit Message: `[AI:Antigravity] Implement Max-Cut on random graphs, QUBO/Ising binary-to-spin mapping, knapsack penalty encoding, and JSON export for Track C`

## Verification Note
1. Correctness Check: Verified algebraic equivalence between QUBO energy and Ising Hamiltonian across all $2^4=16$ spin configurations with error $< 10^{-10}$; verified knapsack penalty term recovered the exact global optimum (val 220, wt 50).
2. Baseline & Fair Comparison: Classical brute-force Max-Cut (optimal cut = 15) compared with Greedy (cut = 14) and 10 deterministic runs of Simulated Annealing ($15 \pm 0$).
3. Claims Verified: Verified that exported JSON in `B_discrete/results/maxcut_qubo.json` contains full upper-triangular QUBO, Ising couplings, and exact ground-truth solutions with zero compiler warnings under `-Wall -Wextra -Wpedantic`.
