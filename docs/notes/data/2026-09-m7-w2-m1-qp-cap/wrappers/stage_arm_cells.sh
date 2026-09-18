#!/usr/bin/env bash
# ONE ARM'S 23 CELLS, one lock acquisition (the granularity the other lane's
# sweep uses), through this task's own serial runner.
set -o pipefail
S=/home/ghecht/Projects/hven/.scratch/m7w2m1
ARM="$1"   # off | budget | retry | both
TAG="$2"   # e.g. budget-r1
python3 -B "$S/run_cells.py" --binary "$S/bench-$ARM/lanes/hven/hven_bench_cell" \
  --tag "$TAG" --out "$S/measure"
