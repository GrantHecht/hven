#!/usr/bin/env bash
# A CANDIDATE CELL, RE-RUN ALONE (CLAUDE.md section 7; run_walk_census.sh:40-44,
# :445-451).  One cell, one pinned physical core, MKL_NUM_THREADS=1, nothing
# else on the box.  Only a deviation REPRODUCED here is a finding.
#
# WHAT IS FOLDED: the solve's own status.  What is NOT: the comparator's, and
# deliberately -- this file holds ONE cell against a 57-cell baseline, so
# census_compare.py necessarily prints 56 MISSING CELL lines and exits 1 on a
# PERFECT agreement.  Its status here would therefore be noise; the MISMATCH
# lines are the reading, and they are printed in full.
#   usage: census_serial_rerun.sh <cell-id>
set -uo pipefail
source /home/ghecht/Projects/hven/.scratch/m6-close/env.sh
BIN=${HVEN_CLOSE_CENSUS_BIN:-$F/build-release/bench/hven_sqp_corpus}
CELL="$1"
D=$F/census/serial
mkdir -p "$D" "$F/logs"
worst=0
taskset -c 0 env MKL_NUM_THREADS=1 "$BIN" --engine walk --cells "$CELL" \
    --csv "$D/$CELL.csv" > "$F/logs/census-serial-$CELL.out" 2>&1
rc=$?; worst=$((worst | rc)); echo "SOLO_RUN_RC=$rc  (worst $worst)"
if [ $rc -ne 0 ]; then echo "CENSUS_SERIAL_WORST_RC=$worst"; exit $worst; fi
python3 -B "$R/scripts/census_compare.py" \
    "$R/bench/baselines/2026-08-16-u0-corpus/walk_baseline.csv" "$D/$CELL.csv" \
    > "$F/logs/census-serial-cmp-$CELL.out" 2>&1
cmp_rc=$?
echo "SOLO_COMPARE_RC=$cmp_rc  (NOT folded -- see the header)"
echo "--- MISMATCH lines for $CELL (none = the deviation did NOT reproduce) ---"
grep -A2 "^MISMATCH $CELL\$" "$F/logs/census-serial-cmp-$CELL.out" || echo "(no MISMATCH line for $CELL)"
grep -c '^MISSING CELL' "$F/logs/census-serial-cmp-$CELL.out" | sed 's/^/MISSING CELL lines (expected 56): /'
echo "CENSUS_SERIAL_WORST_RC=$worst"
exit $worst
