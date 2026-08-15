#!/usr/bin/env bash
# B1 A/B measurement runner. Strictly serial: one solve at a time, machine
# otherwise idle, MKL_NUM_THREADS=1. Every cell is bounded by the corpus
# runner's own per-phase wall deadline; the probe count is fixed.
set -euo pipefail
SP=/tmp/claude-1000/-home-ghecht-Projects-hven/befb7a32-f021-43a2-b05c-b078913d7b67/scratchpad
export MKL_NUM_THREADS=1
CELLS=$(cat "$SP/cells.txt")
REPS=4
PROBE_REPS=7

mkdir -p "$SP/ab"
echo "start $(date -Is)"

for r in $(seq 1 $REPS); do
  for arm in A B; do
    t0=$(date +%s.%N)
    "$SP/build-$arm/bench/hven_sqp_corpus" --engine ssn --cells "$CELLS" \
        --csv "$SP/ab/corpus_${arm}_r${r}.csv" > /dev/null 2>&1
    t1=$(date +%s.%N)
    echo "corpus arm=$arm rep=$r total_s=$(echo "$t1 - $t0" | bc)"
  done
done

for r in $(seq 1 $PROBE_REPS); do
  for arm in A B; do
    t0=$(date +%s.%N)
    "$SP/build-$arm/bench/hven_sqp_ssn_safeguard_probe" census 20000 \
        > "$SP/ab/probe_${arm}_r${r}.out" 2>/dev/null
    t1=$(date +%s.%N)
    echo "probe arm=$arm rep=$r total_s=$(echo "$t1 - $t0" | bc)"
  done
done

echo "done $(date -Is)"
