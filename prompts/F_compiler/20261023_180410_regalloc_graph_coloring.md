# Prompt Log: F_compiler - regalloc_graph_coloring
- **Timestamp**: 2026-10-23 18:04:10
- **Track**: F_compiler
- **AI Tool & Model**: Antigravity (Gemini 3.8 Flash)

## Original Prompt
```text
F3. Formulate register allocation as graph colouring (or pass ordering as a search problem), solve with greedy and with exact DP or ILP, and compare. Measure instruction count, runtime, code size and compile time.
```

## AI Response (Summary / Relevant Excerpt)
```text
1. Mathematical Graph Coloring Formulation:
   - Live Variable Analysis via fixed-point backward dataflow iteration: LiveIn[i] = Use[i] U (LiveOut[i] \ Def[i]), LiveOut[i] = U LiveIn[s].
   - Interference Graph G = (V, E) where vertices are program variables/temporaries and edges represent overlapping liveness intervals.
   - Register Allocation with K physical registers formulated as minimum chromatic vertex coloring or min-spill ILP:
     min sum s_v (spilled variables) subject to x_ui + x_vi <= 1 - s_v for all (u, v) in E.
2. Solvers Implemented in `F_compiler/src/toy_regalloc.hpp`:
   - Method 1: Greedy Kempe / Chaitin-Briggs Simplification Heuristic (degree < K simplification stack + spill metric weighting).
   - Method 2: Exact Branch-and-Bound with DSATUR saturation-degree ordering and symmetry-breaking pruning.
3. Code Generation to Register Machine:
   - Lowers TAC into 3-address register instructions, injecting spill loads and stores for variables mapped to memory stack slots.
4. Comprehensive Benchmark (Target K = 3 Registers):
   - Evaluated across all 15 benchmark programs.
   - Exact BnB reduced total spilled variables from 21 down to 16 (23.8% reduction), saving spill load/store instructions and code size in `p08_fibonacci` (4 -> 2 spills) and `p15_sum_of_squares` (4 -> 1 spill).
   - Trade-off demonstrated: Greedy solves in 378.5 us total (~25 us/program), while Exact BnB guarantees global minimum spills at ~200 ms total compile time.
   - Exported results to `F_compiler/results/register_allocation_benchmark.csv`.
```

## Revised Prompt / User Iteration
```text
N/A
```

## Commit Link
- Commit Hash / Tag: `[AI:Antigravity] Formulate register allocation as graph coloring with Greedy Kempe vs Exact BnB DSATUR`

## Verification Note
1. Correctness Check: Unit tests in `F_compiler_test` verified liveness analysis, interference graph construction, and register validity: confirmed zero conflicting register assignments for all adjacent interference edges in both Greedy and Exact modes.
2. Baseline & Fair Comparison: Greedy Kempe and Exact BnB evaluated on identical interference graphs with identical target register limit (K = 3) across all 15 benchmark programs.
3. Claims Verified: Measured instruction count, spill counts, code size in bytes, and compile time in microseconds/milliseconds; all figures exported and verified in `register_allocation_benchmark.csv`.
