#!/usr/bin/env bash
# THE r3 LEG BODIES.  usage: legs_r3.sh <census|char>   (run by run_leg.sh, inside the box lock).  Outputs under $F3 only.
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HERE/env.sh"
LEG="$1"; mkdir -p "$LOGS3"; cd "$R" || exit 90
case "$LEG" in
 census)
   OUT="$F3/census"; worst=0
   "$BIN" --from-csv "$CENSUS_BASELINE" --csv "$LOGS3/census-stamp.csv" > "$LOGS3/census-stamp.out" 2>&1; rc=$?; worst=$((worst|rc))
   grep -E '^# (binary|schema|budget_table_hash):' "$LOGS3/census-stamp.csv"
   grep -qE '^# binary: [0-9a-f]' "$LOGS3/census-stamp.csv" || { echo "FATAL: no stamp"; exit 2; }
   grep -qE '^# binary:.*-dirty' "$LOGS3/census-stamp.csv" && { echo "FATAL: stamp is -dirty"; exit 2; }
   rm -rf "$OUT"
   bash scripts/run_walk_census.sh --binary "$BIN" --out "$OUT" --compare "$R/scripts/census_compare.py" > "$LOGS3/census-run.out" 2>&1
   rc=$?; echo "CENSUS_RUN_RC=$rc"; worst=$((worst|rc)); tail -12 "$LOGS3/census-run.out" | cut -c1-220
   exit $worst;;
 char)
   # SECTION-7 SOLO CHARACTERIZATION (informational): f7_n10000_path_physics with the hidden per-phase wall budget forced to 7200 s (the
   # binary stamps the forced budget into the row file's provenance WARNING).  Alone on the machine (the lock + the in-lock pgrep), pinned to core 0.
   OUT="$F3/char"; rm -rf "$OUT"; mkdir -p "$OUT"
   echo "CHAR_START $(date -u +%FT%TZ)"
   taskset -c 0 env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 "$BIN" --engine walk --cells f7_n10000_path_physics --internal-force-wall-budget-seconds 7200 --csv "$OUT/f7_n10000_path_physics_7200s.csv" > "$OUT/char.out" 2>&1
   rc=$?; echo "CHAR_RC=$rc"; echo "CHAR_END $(date -u +%FT%TZ)"; tail -5 "$OUT/char.out" | cut -c1-220
   exit $rc;;
 *) echo "unknown leg $LEG"; exit 64;;
esac
