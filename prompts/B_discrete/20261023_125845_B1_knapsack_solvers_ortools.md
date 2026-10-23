# Prompt Log: B_discrete - B1_knapsack_solvers_ortools
- **Timestamp**: 2026-10-23 12:58:45
- **Track**: B_discrete
- **AI Tool & Model**: Antigravity (Gemini 3.8 Flash)

## Original Prompt
```text
B1. Implement 0/1 knapsack in C++ with: greedy (by value/weight), dynamic programming, branch-and-bound with a fractional-relaxation bound, and an ILP formulation solved via OR-Tools (or a hand-written simplex if I can't install it; ask me). Generate random instances of sizes 10, 20, 50, 100, 500 with a fixed seed.
```

## AI Response (Summary / Relevant Excerpt)
```text
Installed Google OR-Tools v9.15 C++ library in the environment and implemented 0/1 Knapsack solvers in B_discrete/src/knapsack.hpp:
1. KnapsackInstance: Generator for reproducible random problem instances of sizes N in {10, 20, 50, 100, 500} using deterministic seed 42.
2. solve_greedy: Density heuristic sorting by value/weight ratio with 1/2-approximation guard against largest single item.
3. solve_dynamic_programming: Exact pseudo-polynomial O(N * W) solver with backtracking reconstruction.
4. solve_branch_and_bound: Best-first search over ratio-ordered items with Dantzig fractional LP relaxation upper bound pruning.
5. solve_ilp: Full Integer Linear Programming formulation mapped to Google OR-Tools MPSolver (with fallback exact LP-relaxation Branch-and-Bound when compiled without OR-Tools).

Added comprehensive tests in B_discrete/tests/test_main.cpp:
- Input validation (empty items, negative capacities, zero/negative weights).
- Hand-calculated textbook 3-item instance (capacity 50, optimal value 220).
- Multi-size scaling suite on N in {10, 20, 50, 100, 500}, verifying that exact solvers (DP, B&B, ILP via OR-Tools) find 100% identical optimal values and strictly obey weight capacity.
```

## Revised Prompt / User Iteration
```text
User requested: "Install and link Google OR-Tools C++ library for the ILP formulation". Downloaded and configured OR-Tools v9.15 x86_64 C++ release tarball in WSL with CMake ortools::ortools target integration.
```

## Commit Link
- Commit Message: `[AI:Antigravity] Implement 0/1 knapsack solvers with Greedy, DP, Branch-and-Bound, and Google OR-Tools ILP`

## Verification Note
1. Correctness Check: Verified exact agreement across DP, Branch-and-Bound, and Google OR-Tools ILP on textbook instance (value 220) and benchmark instances N in {10, 20, 50, 100, 500}.
2. Baseline & Fair Comparison: Identical random instances generated from fixed seed (42); strict feasibility checked ($\sum w_i x_i \le W$) on all algorithms.
3. Claims Verified: Verified that Google OR-Tools MPSolver integrates cleanly and that Greedy strictly respects its 1/2-approximation bound with zero compiler warnings under `-Wall -Wextra -Wpedantic`.
