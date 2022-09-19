#!/usr/bin/env bash
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"
cd "$DIR"

echo "Building Obelisk release binaries..."
make all

echo ""
echo "=== Running VM Micro-Benchmarks ==="
bin/bench_vm

echo ""
echo "=== Running Query Macro-Benchmarks ==="
python3 bench/bench_queries.py
