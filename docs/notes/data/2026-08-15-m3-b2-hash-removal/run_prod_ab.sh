#!/usr/bin/env bash
# B2 P-BENCH, production arm: the B1 A/B runner's corpus half, re-run on the
# SHIPPED post-B2 engine over the same 17-cell list, 4 reps, strictly serial,
# MKL_NUM_THREADS=1, machine otherwise idle. Compared against B1's committed
# corpus_A (base, 3 hashes) and corpus_B (patched, 2 hashes) artifacts.
set -euo pipefail
SP=/tmp/claude-1000/-home-ghecht-Projects-hven/befb7a32-f021-43a2-b05c-b078913d7b67/scratchpad
export MKL_NUM_THREADS=1
CELLS=$(cat /home/ghecht/Projects/hven/docs/notes/data/2026-08-15-m3-b1-hash-cost/cells.txt)
mkdir -p "$SP/prodab"
echo "start $(date -Is)"
for r in 1 2 3 4; do
  t0=$(date +%s.%N)
  "$SP/build-census/bench/hven_sqp_corpus" --engine ssn --cells "$CELLS" \
      --csv "$SP/prodab/corpus_P_r${r}.csv" > /dev/null 2>&1
  t1=$(date +%s.%N)
  echo "corpus arm=P rep=$r total_s=$(echo "$t1 - $t0" | bc)"
done
echo "done $(date -Is)"
