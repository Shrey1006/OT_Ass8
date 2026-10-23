#!/usr/bin/env bash
# ==============================================================================
# init_skeleton.sh - Assignment 8 Repository Skeleton Generator
# Course: Optimization Techniques | Assignment 8: Optimize Anything
# ==============================================================================
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

echo "Initializing Assignment 8 repository skeleton in $ROOT_DIR..."

TRACKS=(
    "A_continuous"
    "B_discrete"
    "C_quantum"
    "D_submodular_parallel"
    "E_case_study"
    "F_compiler"
    "G_xv6"
    "H_dbms"
    "I_quantum_algorithms"
    "J_softeng"
)

# 1. Create track directories: src/, tests/, results/
for track in "${TRACKS[@]}"; do
    mkdir -p "$track/src" "$track/tests" "$track/results"
    
    # Track CMakeLists.txt
    cat <<EOF > "$track/CMakeLists.txt"
cmake_minimum_required(VERSION 3.15)
project(${track} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(EXISTS "\${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
    add_executable(${track}_main src/main.cpp)
    target_include_directories(${track}_main PRIVATE src)
endif()

if(EXISTS "\${CMAKE_CURRENT_SOURCE_DIR}/tests/test_main.cpp")
    add_executable(${track}_test tests/test_main.cpp)
    target_include_directories(${track}_test PRIVATE src)
endif()
EOF

    # Track src/main.cpp placeholder
    if [ ! -f "$track/src/main.cpp" ]; then
        cat <<EOF > "$track/src/main.cpp"
#include <iostream>

int main() {
    std::cout << "[$track] Optimization track initialized." << std::endl;
    return 0;
}
EOF
    fi

    # Track tests/test_main.cpp placeholder
    if [ ! -f "$track/tests/test_main.cpp" ]; then
        cat <<EOF > "$track/tests/test_main.cpp"
#include <iostream>
#include <cassert>

int main() {
    std::cout << "[$track] Running tests..." << std::endl;
    assert(true);
    std::cout << "[$track] All tests passed." << std::endl;
    return 0;
}
EOF
    fi
    
    # Placeholder for git tracking results directory
    touch "$track/results/.gitkeep"
done

# 2. Create prompts/ directory with subfolders
mkdir -p prompts
for track in "${TRACKS[@]}"; do
    mkdir -p "prompts/$track"
    touch "prompts/$track/.gitkeep"
done

# 3. Create report/ directory with markdown file per track
mkdir -p report
for track in "${TRACKS[@]}"; do
    cat <<EOF > "report/${track}.md"
# Track Report: ${track}

## 1. Problem Statement & Inefficiency
- **Domain & Process**: 
- **Observed Bottleneck / Inefficiency**: 
- **Baseline Description**: 

## 2. Mathematical Formulation
- **Decision Variables**: 
- **Objective Function**: 
- **Constraints**: 
- **Assumptions**: 
- **Success Criteria / Metrics**: 

## 3. Algorithm Selection & Chapter 6 Alignment
- **Chosen Method(s)**: 
- **Why suitable**: 
- **Rejected Alternatives**: 

## 4. Experimental Setup & Fair Comparison
- **Dataset / Inputs**: 
- **Hardware & Environment**: 
- **Stopping Criteria**: 
- **Random Seeds (Deterministic)**: 
- **Correctness Check**: 

## 5. Results & Benchmark Data
| Metric | Baseline | Optimized | Unit | Change (%) |
| :--- | :--- | :--- | :--- | :--- |
| Objective Value | | | | |
| Execution Time / Latency | | | | |
| Resource Utilization | | | | |

*Note: For stochastic methods, results must report Mean ± Std over 10+ independent runs.*

## 6. Analysis, Trade-offs & Limitations
- **Speed vs. Accuracy**: 
- **Scalability**: 
- **Failure Cases & Overhead**: 
- **Theoretical vs. Simulated vs. Measured Status**: 
EOF
done

# 4. Create README.md
cat <<'EOF' > README.md
# Assignment 8: Optimize Anything (14-Day Vibe-Coding Challenge)
**Optimization Techniques | Lab Assignment 8**

Discover a process. Formulate the problem. Apply optimization. Prove the improvement.

---

## 🛠️ AI Tools Register
Per assignment requirements, all AI tools and model versions used across tracks are recorded below:

| Tool | Model Version | Tracks | Dates | Kind of Work |
| :--- | :--- | :--- | :--- | :--- |
| Antigravity | Gemini 3.8 Flash | Repo Setup, Tracks A-J | 2026-10-23 to 2026-11-06 | Planning, Code Generation, Debugging, Testing, Documentation |

---

## 📚 Chapter 6 Coverage Checklist
All methods explicitly covered in Chapter 6 are tracked below:

| Area | Algorithm / Technique | Experiment Description | Code Location | Result Summary |
| :--- | :--- | :--- | :--- | :--- |
| **ML Foundations** | ML as Optimization (Loss, Regularization) | Small linear/logistic regression setup | `A_continuous/` | Pending |
| **ML Foundations** | Continuous vs Discrete Optimization | Landscape comparison | `A_continuous/`, `B_discrete/` | Pending |
| **ML Foundations** | Convex vs Non-Convex (Local Min / Saddle) | 2D benchmark functions (e.g. Rastrigin/Saddle) | `A_continuous/` | Pending |
| **ML Foundations** | Basic Quantum Concepts in QML | Parameterized rotation / State encoding | `C_quantum/`, `I_quantum_algorithms/` | Pending |
| **Continuous** | Gradient Descent (GD) | Convex quadratic & logistic loss | `A_continuous/` | Pending |
| **Continuous** | SGD & Mini-batch SGD | Noisy gradient step comparison | `A_continuous/` | Pending |
| **Continuous** | Momentum & NAG (Nesterov) | Ill-conditioned quadratic ravine | `A_continuous/` | Pending |
| **Continuous** | AdaGrad, RMSProp, Adam | Sparse & non-convex optimization | `A_continuous/` | Pending |
| **Continuous** | Newton & Gauss-Newton | Exact Hessian & nonlinear least squares | `A_continuous/` | Pending |
| **Continuous** | Backpropagation | Gradient computation engine | `A_continuous/` | Pending |
| **Continuous** | Curvature Approximations (Fisher, HVPs) | Hessian-vector product & diagonal Fisher | `A_continuous/` | Pending |
| **Continuous** | Convergence (Convex vs Non-Convex) | Rate comparison across condition numbers | `A_continuous/` | Pending |
| **Continuous** | Step-size & Optimizer Tuning | Learning rate sweeps & schedule sensitivity | `A_continuous/` | Pending |
| **Continuous** | Noisy Objectives | Optimization under stochastic noise | `A_continuous/` | Pending |
| **Discrete** | Greedy Methods | Knapsack / Set cover heuristic | `B_discrete/` | Pending |
| **Discrete** | Integer Linear Programming (ILP) | Exact integer programming solver / Branching | `B_discrete/` | Pending |
| **Discrete** | Branch-and-Bound | Exact discrete search with bounding | `B_discrete/` | Pending |
| **Discrete** | Dynamic Programming | Optimal substructure allocation | `B_discrete/` | Pending |
| **Discrete** | Feature Selection / Combinatorial | Combinatorial subset selection | `B_discrete/` | Pending |
| **Discrete** | QUBO & Ising Formulation | Binary-to-spin mapping, penalty constraints | `B_discrete/` | Pending |
| **Quantum** | QAOA (Max-Cut) | Max-Cut on 3-regular graph with QAOA ansatz | `C_quantum/` | Pending |
| **Quantum** | VQE | Ground state energy calculation | `C_quantum/` | Pending |
| **Quantum** | PQC & QNN | Variational circuit classifier | `C_quantum/` | Pending |
| **Quantum** | Parameter-Shift Rule & SPSA | Analytical gradient vs stochastic gradient | `C_quantum/` | Pending |
| **Quantum** | Quantum Natural Gradient (QNG) | Fubini-Study metric tensor optimization | `C_quantum/` | Pending |
| **Quantum** | Hardware Noise & Shot Count Impact | Finite shot variance and depolarization noise | `C_quantum/` | Pending |
| **Quantum** | Barren Plateaus Study | Gradient variance scaling with qubit depth | `C_quantum/` | Pending |
| **Submodular** | Diminishing Returns & Submodularity Test | Marginal gain test on facility location / coverage | `D_submodular_parallel/` | Pending |
| **Submodular** | Greedy Submodular Optimization | $(1 - 1/e)$ approximation verification | `D_submodular_parallel/` | Pending |
| **Parallel** | Load Balancing & Work Stealing | Multi-threaded task scheduling comparison | `D_submodular_parallel/` | Pending |
| **Parallel** | Parallel Speedup & Amdahl/Gustafson | Scaling across 1..N cores / threads | `D_submodular_parallel/` | Pending |
| **Distributed** | FedAvg vs Centralized | Communication rounds vs local step epochs | `D_submodular_parallel/` | Pending |
| **Multi-Task** | MGDA & GradNorm vs Equal Weighting | Pareto multi-objective optimization | `D_submodular_parallel/` | Pending |
| **Case Study** | Main Real-Process Problem | End-to-end domain problem optimization | `E_case_study/` | Pending |
| **Compilers** | Constant Folding / DCE / CSE / Inlining | Pass pipeline optimization | `F_compiler/` | Pending |
| **Compilers** | Register Allocation / Instruction Schedule | Formulated as graph coloring / ILP | `F_compiler/` | Pending |
| **xv6 OS** | Kernel Scheduler / VM / Allocator | Priority / Stride scheduler or COW fork | `G_xv6/` | Pending |
| **DBMS** | Query Rewrite & Join Order Optimization | DP vs Greedy join enumeration | `H_dbms/` | Pending |
| **DBMS** | Index Selection under Storage Budget | Knapsack / ILP index selection | `H_dbms/` | Pending |
| **Quantum Algs**| Deutsch-Jozsa & Bernstein-Vazirani | Oracle algorithm implementations | `I_quantum_algorithms/` | Pending |
| **Quantum Algs**| Grover Search vs Classical Linear Search | Query count and runtime comparison | `I_quantum_algorithms/` | Pending |
| **Quantum Algs**| QFT & Phase Estimation | Fourier transform & eigenvalue estimation | `I_quantum_algorithms/` | Pending |
| **Quantum Algs**| Quantum Kernels vs Classical SVM | Kernel ridge / support vector classification | `I_quantum_algorithms/` | Pending |
| **SoftEng** | Software Engineering Task Optimization | Test suite minimization / build scheduling | `J_softeng/` | Pending |

---

## ⚙️ Setup Instructions Placeholder

### Prerequisites
- C++17 compliant compiler (`g++` >= 9.0, `clang++` >= 10.0, or MSVC 2019+)
- CMake >= 3.15
- Ninja or Make build system
- Git & Bash environment

### Building the Project
```bash
# Configure with CMake
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Build all tracks
cmake --build build

# Run all track tests and benchmarks
bash scripts/run_all.sh
```

---

## 📂 Repository Structure
```
.
├── A_continuous/           # Track A: Continuous optimization
├── B_discrete/             # Track B: Discrete & QUBO optimization
├── C_quantum/              # Track C: Quantum optimization (QAOA, VQE, etc.)
├── D_submodular_parallel/  # Track D: Submodular, parallel & distributed optimization
├── E_case_study/           # Track E: Main real-process case study
├── F_compiler/             # Track F: Compiler optimization
├── G_xv6/                  # Track G: xv6 OS optimization
├── H_dbms/                 # Track H: DBMS optimization
├── I_quantum_algorithms/   # Track I: Quantum algorithms & QML advantage
├── J_softeng/              # Track J: Software engineering optimization
├── prompts/                # Per-track prompt logs
├── report/                 # Technical reports per track
├── scripts/                # Utility & execution scripts
├── failure_log.md          # Documented AI failure cases and corrections
├── tool_comparison.md      # Head-to-head AI tool comparison
├── CMakeLists.txt          # Top-level build file
└── README.md
```
EOF

# 5. Create failure_log.md
cat <<'EOF' > failure_log.md
# AI Failure Log

Per Section 8 of the assignment specification, at least three documented cases where AI-generated code was wrong, unsafe, inefficient, or misleading must be recorded, along with the fix and the lesson learned.

---

### Incident 1: [Short Title]
- **Track**: e.g., Track A (Continuous Optimization)
- **Date**: YYYY-MM-DD
- **AI Tool & Model**: e.g., Antigravity (Gemini 3.8 Flash)
- **Symptom / Manifestation**:
  Describe what went wrong (e.g. incorrect gradient formula, segfault, invalid algorithm assumption, state corruption).
- **Incorrect Code Snippet**:
  ```cpp
  // Code snippet containing the issue
  ```
- **Root Cause Analysis**:
  Explain why the AI produced this error (e.g. subtle edge case, hallucinated API, off-by-one error).
- **The Fix**:
  ```cpp
  // Corrected code snippet
  ```
- **Verification**:
  How was correctness independently proven (unit test, mathematical invariant check)?
- **Lesson Learned**:
  Key takeaway for prompt engineering and verification rigor.

---

### Incident 2: [Short Title]
- **Track**: e.g., Track G (xv6 OS Optimization)
- **Date**: YYYY-MM-DD
- **AI Tool & Model**: 
- **Symptom / Manifestation**:
- **Incorrect Code Snippet**:
- **Root Cause Analysis**:
- **The Fix**:
- **Verification**:
- **Lesson Learned**:

---

### Incident 3: [Short Title]
- **Track**: e.g., Track C (Quantum Optimization)
- **Date**: YYYY-MM-DD
- **AI Tool & Model**: 
- **Symptom / Manifestation**:
- **Incorrect Code Snippet**:
- **Root Cause Analysis**:
- **The Fix**:
- **Verification**:
- **Lesson Learned**:
EOF

# 6. Create tool_comparison.md
cat <<'EOF' > tool_comparison.md
# AI Tool Comparison Study

Per Section 8 of the assignment specification, this document records a head-to-head comparison of at least two AI tools on a representative optimization task.

---

## 1. Benchmark Task Definition
- **Track**: 
- **Task Description**: 
- **Evaluation Criteria**: Correctness, prompt iterations required, hallucination rate, runtime/efficiency of code, code clarity.

---

## 2. Comparison Matrix

| Evaluation Dimension | Tool A: [e.g., Antigravity / Gemini 3.8 Flash] | Tool B: [e.g., Claude 3.5 Sonnet / GPT-4o] |
| :--- | :--- | :--- |
| **First-Pass Correctness** | | |
| **Number of Prompt Iterations**| | |
| **Hallucination / Unsupported Claims**| | |
| **Code Structure & C++17 Idioms** | | |
| **Explanation Quality** | | |
| **Verification & Edge-Case Coverage** | | |

---

## 3. Qualitative Observations & Prompt Transcript
### Tool A Prompt & Response
...

### Tool B Prompt & Response
...

---

## 4. Conclusion & Recommendations
Summary of findings regarding when each tool excels and how verification protocols mitigate weaknesses.
EOF

# 7. Create top-level CMakeLists.txt
cat <<'EOF' > CMakeLists.txt
cmake_minimum_required(VERSION 3.15)
project(Assignment8_OptimizeAnything LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

enable_testing()

# Build all track subdirectories
add_subdirectory(A_continuous)
add_subdirectory(B_discrete)
add_subdirectory(C_quantum)
add_subdirectory(D_submodular_parallel)
add_subdirectory(E_case_study)
add_subdirectory(F_compiler)
add_subdirectory(G_xv6)
add_subdirectory(H_dbms)
add_subdirectory(I_quantum_algorithms)
add_subdirectory(J_softeng)
EOF

# 8. Create scripts/log_prompt.sh
mkdir -p scripts
cat <<'EOF' > scripts/log_prompt.sh
#!/usr/bin/env bash
# ==============================================================================
# log_prompt.sh - Create a timestamped prompt log file for a specific track
# Usage: ./scripts/log_prompt.sh <track_folder_name> [optional_log_name]
# Example: ./scripts/log_prompt.sh A_continuous gd_momentum_test
# ==============================================================================
set -euo pipefail

if [ "$#" -lt 1 ]; then
    echo "Usage: $0 <track_name> [title]"
    echo "Available tracks: A_continuous, B_discrete, C_quantum, D_submodular_parallel, E_case_study, F_compiler, G_xv6, H_dbms, I_quantum_algorithms, J_softeng"
    exit 1
fi

TRACK="$1"
TITLE="${2:-prompt_log}"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
HUMAN_DATE=$(date +"%Y-%m-%d %H:%M:%S")

TARGET_DIR="prompts/$TRACK"

if [ ! -d "$TARGET_DIR" ]; then
    mkdir -p "$TARGET_DIR"
fi

FILENAME="${TARGET_DIR}/${TIMESTAMP}_${TITLE}.md"

cat <<LOGEOF > "$FILENAME"
# Prompt Log: ${TRACK} - ${TITLE}
- **Timestamp**: ${HUMAN_DATE}
- **Track**: ${TRACK}
- **AI Tool & Model**: Antigravity (Gemini 3.8 Flash)

## Original Prompt
\`\`\`text
<Enter original prompt here>
\`\`\`

## AI Response (Summary / Relevant Excerpt)
\`\`\`text
<Enter AI response here>
\`\`\`

## Revised Prompt / User Iteration
\`\`\`text
<Enter revised prompt or user follow-up feedback here>
\`\`\`

## Commit Link
- Commit Hash / Tag: \`[AI:Antigravity] <commit_message_or_hash>\`

## Verification Note
1. Correctness Check: <Independently verified outputs and constraints>
2. Baseline & Fair Comparison: <Identical inputs, stopping criteria, and seeds verified>
3. Claims Verified: <No unsupported speedup or theoretical claims made>
LOGEOF

echo "Created prompt log: $FILENAME"
EOF
chmod +x scripts/log_prompt.sh

# 9. Create scripts/run_all.sh
cat <<'EOF' > scripts/run_all.sh
#!/usr/bin/env bash
# ==============================================================================
# run_all.sh - Build and execute test suites and benchmarks across all tracks
# ==============================================================================
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

echo "========================================================"
echo " Running All Track Tests - Optimization Techniques Ass8"
echo "========================================================"

BUILD_DIR="build"

if [ ! -d "$BUILD_DIR" ]; then
    echo "[INFO] Creating build directory and configuring with CMake..."
    cmake -B "$BUILD_DIR" -S . -DCMAKE_BUILD_TYPE=Release
fi

echo "[INFO] Building all track targets..."
cmake --build "$BUILD_DIR"

echo "[INFO] Executing tests for all available tracks..."
TRACKS=(
    "A_continuous"
    "B_discrete"
    "C_quantum"
    "D_submodular_parallel"
    "E_case_study"
    "F_compiler"
    "G_xv6"
    "H_dbms"
    "I_quantum_algorithms"
    "J_softeng"
)

for track in "${TRACKS[@]}"; do
    TEST_BIN="$BUILD_DIR/$track/${track}_test"
    MAIN_BIN="$BUILD_DIR/$track/${track}_main"

    echo "--------------------------------------------------------"
    echo "Track: $track"
    
    if [ -f "$TEST_BIN" ] || [ -f "${TEST_BIN}.exe" ]; then
        echo "Running test suite..."
        "$TEST_BIN"
    elif [ -f "$MAIN_BIN" ] || [ -f "${MAIN_BIN}.exe" ]; then
        echo "Running main executable..."
        "$MAIN_BIN"
    else
        echo "[WARNING] No executable found for $track in $BUILD_DIR/$track."
    fi
done

echo "========================================================"
echo " All tracks executed successfully."
echo "========================================================"
EOF
chmod +x scripts/run_all.sh

echo "Repository skeleton successfully initialized!"
