#!/usr/bin/env bash
# B2 P-BENCH: re-run B1's arm-S scaffold on the POST-B2 engine, over the 11
# headline cells of the B1 artifact's section 5 table. Strictly serial, one
# solve at a time, machine otherwise idle, MKL_NUM_THREADS=1. Every cell is
# bounded by the corpus runner's own per-phase wall deadline.
set -euo pipefail
SP=/tmp/claude-1000/-home-ghecht-Projects-hven/befb7a32-f021-43a2-b05c-b078913d7b67/scratchpad
BIN="$SP/build-S2/bench/hven_sqp_corpus"
OUT="$SP/pbench"
export MKL_NUM_THREADS=1

CELLS="
f7_n750_path_neutral_control
f7_n800_path_neutral
f7_n800_path_corrupted
f7_n800_path_warm
f7_n825_path_neutral_control
f7_n1000_path_neutral
f7_n1000_path_corrupted
f7_n1000_path_warm
f7_n5000_path_activity
f7_n10000_path_activity
f7_n20000_path_activity
"

mkdir -p "$OUT"
echo "start $(date -Is)"
for c in $CELLS; do
    echo "### $c"
    "$BIN" --engine ssn --cells "$c" --csv "$OUT/$c.csv" 2>&1 >/dev/null |
        grep '^\[B1PROBE\]' || true
done
echo "done $(date -Is)"
