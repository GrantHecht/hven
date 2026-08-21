#!/bin/bash
# Alternating driver: base,head per rep (not all-base then all-head).
set -u
ROOT=/home/ghecht/Projects/hven/.scratch/task-3
BASE=$ROOT/basebuild/bench/hven_sqp_corpus
HEAD=$ROOT/build/bench/hven_sqp_corpus
OUT=$ROOT/corpus/runs
CELLS=$(cat $ROOT/corpus/cells.txt)
mkdir -p "$OUT"
sum_wall() { awk -F, '!/^#/ && $1!="cell_id" {s+=$14} END {printf "%.6f", s}' "$1"; }
for r in 1 2 3 4 5; do
  for side in base head; do
    bin=$BASE; [ "$side" = head ] && bin=$HEAD
    MKL_NUM_THREADS=1 taskset -c 2 "$bin" --engine ssn --cells "$CELLS" \
      --csv "$OUT/ssn17_${side}_${r}.csv" > /dev/null 2>&1
    echo "rep$r $side $(sum_wall "$OUT/ssn17_${side}_${r}.csv")"
  done
done
