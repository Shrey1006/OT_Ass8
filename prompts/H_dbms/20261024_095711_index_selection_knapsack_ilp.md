# Prompt Log: H_dbms - index_selection_knapsack_ilp
- **Timestamp**: 2026-10-24 09:57:11
- **Track**: H_dbms
- **AI Tool & Model**: Antigravity (Gemini 3.8 Flash)

## Original Prompt
```text
H2. Formulate index selection as a knapsack/ILP with a storage budget. Write a C++ program that reads candidate indexes with estimated benefit and size (from EXPLAIN) and solves them with greedy and DP, and ILP if possible.
```

## AI Response (Summary / Relevant Excerpt)
```text
1. Mathematical 0/1 Knapsack & ILP formulation under storage budget B.
2. Candidate index extraction script measuring actual PostgreSQL 16 EXPLAIN (FORMAT JSON) plan cost reductions and pg_relation_size footprints across TPC-H tables.
3. C++ engine implementing Greedy (density-based), Dynamic Programming (0/1 Knapsack), and Exact ILP (Branch-and-Bound with Continuous Knapsack LP relaxation).
4. Full comparison suite evaluated across budgets 1MB, 2.5MB, 5MB, 10MB, 15MB, 25MB and verified via 6 unit test invariants.
```

## Revised Prompt / User Iteration
```text
Initial implementation of Step H2.
```

## Commit Link
- Commit Hash / Tag: `[AI:Antigravity] Formulate index selection as knapsack/ILP under storage budget with Greedy, DP, and Branch-and-Bound solvers`

## Verification Note
1. Correctness Check: Verified DP and ILP produce identical global optimal benefits across all budget regimes and match textbook edge cases.
2. Baseline & Fair Comparison: Evaluated across identical candidate datasets extracted from PostgreSQL EXPLAIN query costs.
3. Claims Verified: Verified storage capacity constraint sum(s_i * x_i) <= B is strictly enforced with zero overflow.
