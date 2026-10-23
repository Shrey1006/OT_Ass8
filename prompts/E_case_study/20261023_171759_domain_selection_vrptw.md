# Prompt Log: E_case_study - domain_selection_vrptw
- **Timestamp**: 2026-10-23 17:17:59
- **Track**: E_case_study
- **AI Tool & Model**: Antigravity (Gemini 3.8 Flash)

## Original Prompt
```text
E0. I need a Track E domain that is not compiler, OS, DBMS, quantum or software engineering. Suggest 5 options that have real or easily generated data, and for each give the objective, constraints and why it suits Chapter 6. Then ask me to pick.
```

## AI Response (Summary / Relevant Excerpt)
```text
Presented 5 distinct, non-overlapping domains for Track E with complete mathematical objectives, realistic constraints, and Chapter 6 optimization algorithm mapping:
1. Smart Grid Microgrid Energy Dispatch (QP / LP with battery degradation & solar arbitrage)
2. Financial Portfolio Optimization (Markowitz Mean-Variance + CVaR with cardinality constraints)
3. Last-Mile Drone / Electric Vehicle Fleet Routing with Time Windows & Battery Recharging (EV-VRPTW)
4. Supply Chain Multi-Echelon Facility Location & Non-Linear Safety Stock Allocation
5. 5G Wireless Beamforming & Power Allocation (WMMSE & non-convex SINR optimization)

User Selected:
Option 3: Last-Mile Drone / EV Fleet Routing (VRPTW).
- Objective: Minimize total fleet travel distance, vehicle fixed deployment cost, and tardiness penalty.
- Constraints: Vehicle payload capacity, customer delivery time windows, battery discharge rates, and recharge station detours.
- Chapter 6 Alignment: Mixed-integer linear programming (ILP) formulation, nearest-neighbor / savings heuristic baselines, metaheuristics (Simulated Annealing / Genetic Algorithm with 2-opt local search), and continuous trajectory / speed relaxation.
```

## Revised Prompt / User Iteration
```text
User selected Option 3 via interactive modal: Last-Mile Drone / EV Fleet Routing (VRPTW).
```

## Commit Link
- Commit Hash / Tag: `[AI:Antigravity] Select Track E domain: Last-Mile Drone and Electric Vehicle Fleet Routing (EV-VRPTW)`

## Verification Note
- **Domain Eligibility**: Confirmed non-overlap with prohibited domains (not OS, compiler, DBMS, quantum, or software engineering).
- **Mathematical Specification**: Outlined objective (distance + vehicle costs + tardiness), physical constraints (payload, time windows, battery state of charge), and benchmark data generation.
- **Chapter 6 Mapping**: Direct alignment with combinatorial routing (ILP), submodular insertion heuristics, and metaheuristic local search (SA/GA with 2-opt).
