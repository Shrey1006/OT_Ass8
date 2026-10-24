# Prompt Log: I_quantum_algorithms - grover_vs_linear_search_and_noise
- **Timestamp**: 2026-10-24 17:17:50
- **Track**: I_quantum_algorithms
- **AI Tool & Model**: Antigravity (Gemini 3.8 Flash)

## Original Prompt
```text
I3. Compare Grover with linear search using query counts, gate counts and wall-clock time. Run one noisy-simulation experiment. Write the analysis: when quantum helps and when it doesn't, contrasted with Track D speedups. An honest negative result is fine.
```

## AI Response (Summary / Relevant Excerpt)
```text
1. Implemented scaling comparison between Grover's algorithm and classical linear search across n in {3, 4, 5, 6, 7, 8} (N in {8, 16, 32, 64, 128, 256}).
2. Metrics compared:
   - Query counts: O(sqrt(N)) vs O(N) showing a 13.92x speedup at N=256 (12 Grover queries vs 167 classical queries).
   - Gate counts: Transpiled elementary gates (u, cx) vs classical ALU operations. At N=256, classical requires 334 ALU ops vs 8,804 quantum gates (4,032 CX gates).
   - Wall-clock time: Classical CPU takes 8.52 us vs 498.64 us estimated on physical superconducting hardware (classical is 58.5x faster in wall-clock time).
3. Conducted a depolarizing noise experiment on n=3, showing success probability degrading monotonically from 94.29% (noise-free) to 13.57% (p1=5%, p2=10%), collapsing to the uniform random guessing baseline (12.5%).
4. Documented an honest analysis in report/I_quantum_algorithms.md contrasting Grover's constant-factor trap and noise sensitivity against Track D's deterministic 3.8x parallel speedup on 4 CPU threads.
```

## Revised Prompt / User Iteration
```text
None (Initial prompt implemented and verified).
```

## Commit Link
- Commit Hash / Tag: `[AI:Antigravity] Compare Grover vs linear search on queries, gates, clock time and noisy simulation`
