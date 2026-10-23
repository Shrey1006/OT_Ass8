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
