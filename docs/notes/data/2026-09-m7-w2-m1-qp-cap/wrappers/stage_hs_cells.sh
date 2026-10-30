#!/usr/bin/env bash
# ONE ARM'S hs TRAIN SPLIT, one lock acquisition.
set -o pipefail
S=/home/ghecht/Projects/hven/.scratch/m7w2m1
ARM="$1"; TAG="$2"
python3 -B "$S/run_hs_cells.py" --binary "$S/bench-$ARM/lanes/hven/hven_bench_cell" \
  --tag "$TAG" --out "$S/measure-hs"
