#!/usr/bin/env bash
# ==============================================================================
# generate_tpch.sh - Build dbgen and generate TPC-H SF 0.1 dataset
# Usage: ./H_dbms/scripts/generate_tpch.sh [scale_factor]
# ==============================================================================
set -euo pipefail

SF="${1:-0.1}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TRACK_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
TOOL_DIR="${TRACK_DIR}/tpch-tool"
DATA_DIR="${TRACK_DIR}/data"

echo "========================================================="
echo " [Step 2] Generating TPC-H Dataset (Scale Factor = ${SF})"
echo "========================================================="

mkdir -p "${DATA_DIR}"

if [ ! -f "${TOOL_DIR}/dbgen" ]; then
    echo "Cloning and building TPC-H dbgen..."
    mkdir -p "${TOOL_DIR}"
    if [ ! -d "${TOOL_DIR}/.git" ]; then
        git clone https://github.com/electrum/tpch-dbgen.git "${TOOL_DIR}"
    fi
    cd "${TOOL_DIR}"
    make -j$(nproc 2>/dev/null || echo 2)
else
    echo "TPC-H dbgen executable already built."
fi

echo "Running dbgen with scale factor ${SF}..."
cd "${TOOL_DIR}"
./dbgen -s "${SF}" -f

echo "Moving generated tables to ${DATA_DIR}..."
mv *.tbl "${DATA_DIR}/"

echo "Cleaning trailing pipe delimiters for PostgreSQL COPY format..."
cd "${DATA_DIR}"
for tbl in *.tbl; do
    sed -i 's/|$//' "$tbl"
done

echo "TPC-H dataset generation completed successfully:"
ls -lh "${DATA_DIR}"/*.tbl
