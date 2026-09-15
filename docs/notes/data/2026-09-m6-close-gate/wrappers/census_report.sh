#!/usr/bin/env bash
# The census's per-tier reading, and the 13 formerly-discharged cells BY NAME.
# Pure reporting over the runner's own output; it runs nothing and asserts nothing.
set -uo pipefail
R=/home/ghecht/Projects/hven
M=$R/.scratch/m6-close
RUN=${1:-$M/census/run}
BASE=$R/bench/baselines/2026-08-16-u0-corpus/walk_baseline.csv
MERGED=$RUN/walk_census.csv

T2="f7_n10000_path_physics f7_n5000_path_neutral f7_n5000_path_warm"
T3="f7_n10000_path_activity f7_n10000_path_corrupted f7_n10000_path_neutral \
f7_n10000_path_warm f7_n20000_path_activity f7_n20000_path_corrupted \
f7_n20000_path_neutral f7_n20000_path_physics f7_n20000_path_warm \
f7_n5000_path_corrupted"

col() { awk -F, -v c="$1" -v n="$2" '!/^#/ && $1==c {print $n; exit}' "$3"; }

echo "=== rows produced ==="
ls "$RUN/rows" | wc -l
echo "=== comparator verdict (the closing tally) ==="
tail -1 "$RUN/compare.txt"
echo "=== MISMATCH / MISSING / EXTRA lines ==="
grep -E '^(MISMATCH|MISSING CELL|EXTRA CELL) ' "$RUN/compare.txt" || echo "(none)"
echo "=== serial-confirm list (non-comment lines) ==="
grep -v '^#' "$RUN/serial_confirm_list.txt" || echo "(none)"
echo
echo "=== THE 13 FORMERLY-DISCHARGED CELLS, BY NAME ==="
printf '%-32s %-16s %-16s %-14s %-14s %s\n' cell base_status fresh_status base_wall_s fresh_wall_s verdict
for tier in T2 T3; do
  eval "cells=\$$tier"
  for c in $cells; do
    bs=$(col "$c" 7 "$BASE"); fs=$(col "$c" 7 "$MERGED")
    bw=$(col "$c" 14 "$BASE"); fw=$(col "$c" 14 "$MERGED")
    if grep -q "^MISMATCH $c\$" "$RUN/compare.txt" 2>/dev/null; then v="MISMATCH"
    elif [ -z "$fs" ]; then v="NO ROW"
    else v="match (13/13)"; fi
    printf '%-32s %-16s %-16s %-14s %-14s %s\n' "$c" "${bs:-?}" "${fs:-<none>}" "${bw:-?}" "${fw:-<none>}" "$v"
  done
done
echo
echo "(base_wall_s / fresh_wall_s are INFORMATIONAL -- excluded from the comparison,"
echo " never quoted as a timing, CLAUDE.md section 7.)"
echo
echo "=== per-tier tally ==="
for tier in 1 2 3; do
  # the tier table's DATA rows are indented three spaces after the tier tag;
  # the one-line tier DESCRIPTION above each block has a single space, and must
  # not be counted (it was, in the first cut of this script).
  n=$(grep -cE "^# census\.  T$tier   " "$MERGED")
  echo "T$tier cells in the merged provenance tier table: $n"
done
echo "(the runner's own counts: line below is the authority)"
echo "=== stamps ==="
grep -E '^# census\.binary_stamp: (binary|schema|budget_table_hash):' "$MERGED"
grep -m1 -E '^# census\.protocol:' "$MERGED"
grep -m1 -E '^# census\.topology:' "$MERGED"
grep -m1 -E '^# census\.  counts:' "$MERGED"
