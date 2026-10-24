# Prompt Log: G_xv6 - benchmark_suite_turnaround_fairness
- **Timestamp**: 2026-10-24 09:13:53 UTC
- **Track**: G_xv6
- **AI Tool & Model**: Antigravity (Gemini 3.8 Flash)

## Original Prompt
```text
G3. Write three user-level benchmark programs (CPU-bound, I/O-bound, mixed) that report turnaround time and fairness. Give the exact commands to run before and after the change.
```

## AI Response (Summary / Relevant Excerpt)
```text
1. Developed three user-level benchmark programs in xv6 RISC-V:
   - user/bench_cpu.c: Compute-bound workload executing 25M arithmetic iterations per worker.
   - user/bench_io.c: I/O-bound workload performing 120 cyclic file create/write/reopen/read/close transactions.
   - user/bench_mixed.c: Mixed workload alternating 30 iterations between 600K arithmetic computations and disk I/O.
2. Metrics Reported:
   - Turnaround Time (ticks): Elapsed timer ticks from synchronized barrier start to completion.
   - Normalized Progress Rate: Rate per assigned ticket x_i = 10000 / (w_i * TT_i).
   - Raw Jain's Fairness Index (Equal share turnaround): J_raw = (sum R_i)^2 / (n * sum R_i^2).
   - Weighted Jain's Fairness Index (Proportional share): J_weighted = (sum x_i)^2 / (n * sum x_i^2).
3. Provided exact commands to test Before (Round-Robin) and After (Lottery Scheduler) using the make SCHED=RR toggle and CPUS=1 contention.
```

## Commit Link
- Commit Hash / Tag: `[AI:Antigravity] Implement CPU, I/O, and mixed benchmarks measuring turnaround and fairness`
