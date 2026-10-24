#!/usr/bin/env bash
# ==============================================================================
# load_tpch.sh - Load TPC-H data into PostgreSQL database tpch_sf01
# Usage: ./H_dbms/scripts/load_tpch.sh [database_name]
# ==============================================================================
set -euo pipefail

DBNAME="${1:-tpch_sf01}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TRACK_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
DATA_DIR="${TRACK_DIR}/data"
SCHEMA_FILE="${SCRIPT_DIR}/schema.sql"

echo "========================================================="
echo " [Step 3] Loading TPC-H Dataset into Database: ${DBNAME}  "
echo "========================================================="

if [ ! -d "${DATA_DIR}" ] || [ ! -f "${DATA_DIR}/lineitem.tbl" ]; then
    echo "Error: TPC-H .tbl files not found in ${DATA_DIR}. Run generate_tpch.sh first."
    exit 1
fi

echo "Creating schema from ${SCHEMA_FILE}..."
sudo -u postgres psql -d "${DBNAME}" -f "${SCHEMA_FILE}"

echo "Bulk loading tables via psql \copy..."
sudo -u postgres psql -d "${DBNAME}" <<EOF
\copy region FROM '${DATA_DIR}/region.tbl' WITH (FORMAT csv, DELIMITER '|');
\copy nation FROM '${DATA_DIR}/nation.tbl' WITH (FORMAT csv, DELIMITER '|');
\copy part FROM '${DATA_DIR}/part.tbl' WITH (FORMAT csv, DELIMITER '|');
\copy supplier FROM '${DATA_DIR}/supplier.tbl' WITH (FORMAT csv, DELIMITER '|');
\copy partsupp FROM '${DATA_DIR}/partsupp.tbl' WITH (FORMAT csv, DELIMITER '|');
\copy customer FROM '${DATA_DIR}/customer.tbl' WITH (FORMAT csv, DELIMITER '|');
\copy orders FROM '${DATA_DIR}/orders.tbl' WITH (FORMAT csv, DELIMITER '|');
\copy lineitem FROM '${DATA_DIR}/lineitem.tbl' WITH (FORMAT csv, DELIMITER '|');
ANALYZE;
EOF

echo ""
echo "========================================================="
echo " Verifying Loaded Row Counts in ${DBNAME}:                "
echo "========================================================="
sudo -u postgres psql -d "${DBNAME}" -c "
SELECT
    'region' AS table_name, count(*) AS row_count FROM region
UNION ALL
SELECT 'nation', count(*) FROM nation
UNION ALL
SELECT 'supplier', count(*) FROM supplier
UNION ALL
SELECT 'customer', count(*) FROM customer
UNION ALL
SELECT 'part', count(*) FROM part
UNION ALL
SELECT 'partsupp', count(*) FROM partsupp
UNION ALL
SELECT 'orders', count(*) FROM orders
UNION ALL
SELECT 'lineitem', count(*) FROM lineitem
ORDER BY row_count ASC;
"

echo "TPC-H SF 0.1 load complete and verified."
