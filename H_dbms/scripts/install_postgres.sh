#!/usr/bin/env bash
# ==============================================================================
# install_postgres.sh - Automated PostgreSQL Installation and Setup
# Usage: ./H_dbms/scripts/install_postgres.sh
# ==============================================================================
set -euo pipefail

echo "========================================================="
echo " [Step 1] Checking and Installing PostgreSQL             "
echo "========================================================="

if ! command -v psql &> /dev/null; then
    echo "PostgreSQL not detected. Installing via apt-get..."
    sudo apt-get update
    sudo apt-get install -y postgresql postgresql-contrib git build-essential
else
    echo "PostgreSQL client and server already installed."
fi

# Ensure PostgreSQL service is active
echo "Ensuring PostgreSQL service is running..."
sudo service postgresql start || true

# Set up database user and create database tpch_sf01
echo "Setting up 'tpch_sf01' database..."
sudo -u postgres psql -c "CREATE USER $(whoami) WITH SUPERUSER;" 2>/dev/null || true
sudo -u postgres psql -c "DROP DATABASE IF EXISTS tpch_sf01;"
sudo -u postgres psql -c "CREATE DATABASE tpch_sf01;"

echo "PostgreSQL setup complete. Database 'tpch_sf01' is ready."
