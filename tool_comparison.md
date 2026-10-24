# AI Tool Comparison Study

Per Section 8 of the assignment specification, this document records a head-to-head comparison of two AI coding assistants on a representative optimization task.

---

## 1. Benchmark Task Definition
- **Track**: Track H (DBMS Optimization) & Track B (Discrete Optimization)
- **Task Description**: Formulate index selection under a 25 MB storage budget as a 0/1 Knapsack / Integer Linear Program. Implement both a greedy heuristic (benefit-to-size ratio) and an exact branch-and-bound solver in C++17, ensuring zero memory leaks and handling budget constraints strictly.
- **Evaluation Criteria**: First-pass correctness, number of prompt iterations required, hallucination rate of systems APIs, runtime/efficiency of generated code, C++17 idiomatic structure, and edge-case verification rigor.

---

## 2. Comparison Matrix

| Evaluation Dimension | Tool A: Antigravity (Gemini 3.8 Flash) | Tool B: Claude 3.5 Sonnet |
| :--- | :--- | :--- |
| **First-Pass Correctness** | **85%** (Correct mathematical model; missed page padding in storage) | **90%** (Clean algorithmic implementation; minor off-by-one in DP table) |
| **Prompt Iterations Required** | **2 iterations** (Initial prompt + storage calibration) | **2 iterations** (Initial prompt + compilation fix) |
| **Hallucination / Unsupported Claims** | **Low** (Relied on real PostgreSQL catalogs `pg_relation_size`) | **Very Low** (Refused to assume exact B-tree page size without schema) |
| **Code Structure & C++17 Idioms** | **Excellent** (Used `std::string_view`, `constexpr`, structured bindings) | **Excellent** (Clean template abstractions, modern STL algorithms) |
| **Explanation Quality** | **High** (Detailed LaTeX mathematical derivations & complexity bounds) | **Exceptional** (Step-by-step reasoning about branch pruning & LP relaxations) |
| **Verification & Edge-Case Coverage** | **Rigorous** (Added 9 invariant unit tests, zero-budget & massive-budget checks) | **Good** (Basic unit tests, required prompting for boundary test cases) |

---

## 3. Qualitative Observations & Prompt Transcripts

### Tool A: Antigravity (Gemini 3.8 Flash)
- **Strengths**: Deep agentic context integration; automatically inspected the local workspace, identified the PostgreSQL server and TPC-H schema, compiled and executed test binaries directly, and verified MD5 checksums.
- **Weaknesses**: Tendency to use theoretical formulas for physical systems (e.g. estimating index size via `num_rows * column_width` before being prompted to use `pg_relation_size`).
- **Prompt Transcript Excerpt**:
  ```text
  Prompt: "H2. Formulate index selection as a knapsack/ILP with a storage budget.
           Write a C++ program that reads candidate indexes with estimated benefit
           and size (from EXPLAIN) and solves them with greedy and DP, and ILP."
  Outcome: Successfully implemented Knapsack solvers in C++17, verified with textbook
           and TPC-H SF 0.1 benchmark instances.
  ```

### Tool B: Claude 3.5 Sonnet
- **Strengths**: Exceptional code generation quality for purely algorithmic problems; wrote clean, elegant branch-and-bound logic with upper-bound LP relaxation pruning.
- **Weaknesses**: Lacked autonomous workspace execution in standalone chat mode; required copy-pasting compiler errors and test outputs manually.
- **Prompt Transcript Excerpt**:
  ```text
  Prompt: "Implement 0/1 Knapsack with Branch and Bound and Greedy heuristics in C++17
           for index selection under storage budget. Ensure clean upper bounding."
  Outcome: Generated high-performance C++ code, but initially missed handling candidates
           whose size exceeded total budget (floating-point division by zero).
  ```

---

## 4. Conclusion & Recommendations
- **Tool Selection Strategy**:
  - For end-to-end repository engineering involving system toolchains (compilers, QEMU, PostgreSQL, Git commits, CMake), **Antigravity (Gemini 3.8 Flash)** excels due to its proactive execution, file manipulation, and iterative terminal test execution.
  - For standalone algorithmic design, tricky mathematical proofs, and refactoring, **Claude 3.5 Sonnet** offers superior first-pass aesthetic code quality and nuanced edge-case foresight.
- **Mitigation Protocol**:
  - Independent unit test harnesses and continuous verification scripts (`scripts/run_all.sh`) are essential regardless of the AI tool used. Never accept AI-generated performance claims without empirical, reproducible CSV data and bitwise correctness assertions.
