#!/usr/bin/env bash
# M7 W2 M3c PASS 2 -- STAGE 1: one leg per traced cell at `--capture-solve -1`.
#
# RULE-P2.md governs and was committed before this ran. DEFAULT options: the
# probe is never given `--tr-init`, so `SqpOptions::tr_init` is not assigned at
# all. One solver process at a time, the box lock per leg, released between
# legs. Everything after this chain -- the mapping, the selection, the slicing
# and the read-outs -- is OFFLINE and runs no solver.
set -o pipefail

ROOT=/home/ghecht/Projects/hven
BENCH=/home/ghecht/Projects/hven-bench
D="$ROOT/docs/notes/data/2026-09-m7-w2-m3c-default-census"
S="$ROOT/.scratch/m7w2m3c/p2"
PROBE="$S/probebuild/m3c_p2_probe"
LOCK=/tmp/box-build.lock

mkdir -p "$S/out" "$S/traces" "$S/logs"

# EVERY PATH HANDED ON IS CHECKED TO EXIST (M3b C8).
for p in "$PROBE" "$D/RULE-P2.md" "$D/rows" "$BENCH/.scratch/h3/decoded"; do
    [ -e "$p" ] || { echo "REFUSED: missing $p"; exit 90; }
done

echo "CHAIN_P2_START $(date -Is)"
echo "=== HEAD $(git -C "$ROOT" rev-parse --short HEAD)  dirty-files=$(git -C "$ROOT" status --porcelain | wc -l)"
echo "=== probe sha256: $(cat "$S/probe-sha256.txt")"

# RULE-P2.md section 3, stage 1: the ten cells, cheapest first (pass-1 wall,
# INFORMATIONAL -- it orders the legs and asserts nothing).
# The three-field tuple is: cell, solver deadline, why that deadline.
LEGS="
ACOPP57:600:group E
DALLASM:600:group E
STEENBRB:600:group E
ACOPP118:600:group E
ACOPR57:600:group E
STEENBRC:600:group E
STEENBRG:600:group E
YORKNET:600:group I
SAWPATH:300:group W -- pass 1 ran it at 300 s and the identity gate compares against that row
BRITGAS:600:group E
"

leg() {
    cell=$1; deadline=$2; why=$3
    echo
    echo "PGREP_BEFORE_LOCK $cell: $(pgrep -a -f 'hven_bench_cell|m3c_probe|m3c_p2_probe|hven_sqp_corpus' | grep -v 'eval \|pgrep' | head -3)"
    flock "$LOCK" bash -c '
      set -o pipefail
      cell='"$cell"'; deadline='"$deadline"'
      S='"$S"'; PROBE='"$PROBE"'; BENCH='"$BENCH"'
      echo "FOREGROUND_START leg-$cell $(date -Is)"
      if pgrep -a -f "hven_bench_cell|m3c_probe|m3c_p2_probe|hven_sqp_corpus" | grep -v "eval \|pgrep"; then
        echo "FOREIGN CELL BINARY UNDER MY LOCK -- SKIPPING $cell"
        echo "FOREGROUND_END leg-$cell $(date -Is) rc=99"
        exit 99
      fi
      echo "PGREP_EMPTY leg-$cell: no foreign cell binary under my lock"
      cd "$BENCH" || exit 93
      t0=$(date +%s.%N)
      MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
        timeout 900 "$PROBE" \
          --problem "$cell" \
          --deadline "$deadline" \
          --capture-solve -1 \
          --max-records 2000000 \
          --trace on \
          --trace-out "$S/traces/$cell-all.csv" \
          --stop-after-first-qp off \
          --ledger on \
          > "$S/out/$cell.p2.txt" 2> "$S/out/$cell.p2.stderr"
      rc=$?
      t1=$(date +%s.%N)
      echo "LEG $cell   deadline=$deadline   rc=$rc   wall_s=$(echo "$t1 - $t0" | bc) (INFORMATIONAL)"
      if [ $rc -ne 0 ]; then
        # RULE-P2.md section 4: CENSORED -- no summary, no trace, no row, and
        # nothing filled in later.
        echo "CENSORED $cell: rc=$rc -- the row and the trace are DELETED and are never reconstructed"
        head -3 "$S/out/$cell.p2.stderr" 2>/dev/null
        rm -f "$S/out/$cell.p2.txt" "$S/traces/$cell-all.csv"
      else
        for k in status major_iters qp_minor_iters factorizations deadline_fired \
                 tr_init_touched capture_solve_arg summary_solves_seen summary_records \
                 summary_truncated summary_allocation_failed; do
          printf "    %s\n" "$(grep -m1 "^$k=" "$S/out/$cell.p2.txt" || echo "$k=ABSENT")"
        done
        echo "    trace_bytes=$(stat -c %s "$S/traces/$cell-all.csv" 2>/dev/null || echo 0)"
      fi
      echo "FOREGROUND_END leg-$cell $(date -Is) rc=$rc"
      exit $rc
    '
    echo "WRAPPER_RC leg-$cell rc=$?  ($why)"
}

for spec in $LEGS; do
    [ -z "$spec" ] && continue
    cell=${spec%%:*}
    rest=${spec#*:}
    deadline=${rest%%:*}
    why=${rest#*:}
    leg "$cell" "$deadline" "$why"
done

echo
echo "=== STAGE 1 LEGS DONE. Rows that exist:"
ls -1 "$S/out"/*.p2.txt 2>/dev/null | wc -l | sed 's/^/    rows: /'
ls -1 "$S/traces"/*-all.csv 2>/dev/null | wc -l | sed 's/^/    traces: /'
du -sh "$S/traces" 2>/dev/null | sed 's/^/    trace bytes: /'
echo
echo "NOTE: the mapping (RULE-P2 section 2), the identity gate (section 5), the"
echo "      selection (section 3 stage 2) and the read-outs (section 6) are"
echo "      OFFLINE and run NO solver. They are not this chain's work."
echo "CHAIN_P2_END $(date -Is)"
