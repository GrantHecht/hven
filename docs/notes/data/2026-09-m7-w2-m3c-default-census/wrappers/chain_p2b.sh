#!/usr/bin/env bash
# M7 W2 M3c PASS 2B -- the RE-TAKE of pass 2's ten legs under ARM C's build.
#
# WHY THIS EXISTS. RULE-P2.md section 5.1's identity gate stopped all ten pass-2
# cells. RULE-P2A's three-arm experiment found the cause: the pass-2 probe was
# configured with CMake's default c++ rather than the clang++ the census's
# install build used, and every Clang-conditional flag vanished with it. All
# three arms of that experiment reproduce the committed pass-1 rows on every
# compared field with zero near-ulp acceptances, which is RULE-P2A section 6's
# OUTCOME 1: re-take the ten legs under arm C's build.
#
# WHAT IS UNCHANGED. RULE-P2.md section 3 stage 1 and section 4 govern these legs
# exactly as they governed pass 2's: the same ten cells in the same order, DEFAULT
# options (`--tr-init` is never passed, so SqpOptions::tr_init is not assigned at
# all), `--capture-solve -1`, `--max-records 2000000`, a 600 s solver deadline --
# 300 s for SAWPATH, whose pass-1 row was taken at 300 s -- inside `timeout 900`,
# one solver process at a time, the box lock per leg, MKL_NUM_THREADS=1. A leg the
# outer stop kills is CENSORED: its row and its trace are DELETED and are never
# reconstructed. RULE-P2.md is NOT edited; only the BINARY under it is repaired.
#
# WHAT IS NEW. The identity gate run per leg is P2A's (wrappers/gate_p2a.py):
# every field the two rows share, byte equality outside the residual class, the
# same 1e-12 bar inside it, ABSENT never counted as agreement. The pass-2 outputs
# are left exactly where they are, in .scratch/m7w2m3c/p2/, as the evidence of
# what the differently-built binary produced; nothing here overwrites them.
set -o pipefail

ROOT=/home/ghecht/Projects/hven
BENCH=/home/ghecht/Projects/hven-bench
D="$ROOT/docs/notes/data/2026-09-m7-w2-m3c-default-census"
S="$ROOT/.scratch/m7w2m3c/p2b"
PROBE="$ROOT/.scratch/m7w2m3c/p2a/armB/m3c_p2a_probe_armB"
LOCK=/tmp/box-build.lock

mkdir -p "$S/rows" "$S/traces" "$S/logs"

for p in "$PROBE" "$D/RULE-P2.md" "$D/RULE-P2A.md" "$D/rows" \
         "$D/wrappers/gate_p2a.py" "$BENCH/.scratch/h3/decoded"; do
    [ -e "$p" ] || { echo "REFUSED: missing $p"; exit 90; }
done

echo "CHAIN_P2B_START $(date -Is)"
echo "=== HEAD $(git -C "$ROOT" rev-parse --short HEAD)  dirty-files=$(git -C "$ROOT" status --porcelain | wc -l)"
echo "=== probe (arm C = arm B's binary, armed): $(sha256sum "$PROBE")"

# `cell:deadline` and NOTHING ELSE on the token (pass 2's word-split defect's
# guard, kept). The order is pass 2's own: cheapest first by pass-1 wall, which
# is INFORMATIONAL and asserts nothing.
LEGS="
ACOPP57:600
DALLASM:600
STEENBRB:600
ACOPP118:600
ACOPR57:600
STEENBRC:600
STEENBRG:600
YORKNET:600
SAWPATH:300
BRITGAS:600
"

leg() {
    cell=$1; deadline=$2
    echo
    echo "PGREP_BEFORE_LOCK $cell: $(pgrep -a -f 'hven_bench_cell|m3c_probe|m3c_p2_probe|m3c_p2a_probe|hven_sqp_corpus' | grep -v 'eval \|pgrep' | head -3)"
    flock "$LOCK" bash -c '
      set -o pipefail
      cell='"$cell"'; deadline='"$deadline"'
      S='"$S"'; PROBE='"$PROBE"'; BENCH='"$BENCH"'
      echo "FOREGROUND_START leg-$cell $(date -Is)"
      if pgrep -a -f "hven_bench_cell|m3c_probe|m3c_p2_probe|m3c_p2a_probe|hven_sqp_corpus" | grep -v "eval \|pgrep"; then
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
          > "$S/rows/C-$cell.txt" 2> "$S/rows/C-$cell.stderr"
      rc=$?
      t1=$(date +%s.%N)
      echo "LEG $cell   deadline=$deadline   rc=$rc   wall_s=$(echo "$t1 - $t0" | bc) (INFORMATIONAL)"
      if [ $rc -ne 0 ]; then
        echo "CENSORED $cell: rc=$rc -- the row and the trace are DELETED and are never reconstructed"
        head -3 "$S/rows/C-$cell.stderr" 2>/dev/null
        rm -f "$S/rows/C-$cell.txt" "$S/traces/$cell-all.csv"
      else
        for k in status major_iters qp_minor_iters factorizations deadline_fired \
                 tr_init_touched capture_solve_arg summary_solves_seen summary_records \
                 summary_truncated summary_allocation_failed; do
          printf "    %s\n" "$(grep -m1 "^$k=" "$S/rows/C-$cell.txt" || echo "$k=ABSENT")"
        done
        echo "    trace_bytes=$(stat -c %s "$S/traces/$cell-all.csv" 2>/dev/null || echo 0)"
      fi
      echo "FOREGROUND_END leg-$cell $(date -Is) rc=$rc"
      exit $rc
    '
    echo "WRAPPER_RC leg-$cell rc=$?"
}

for spec in $LEGS; do
    [ -z "$spec" ] && continue
    cell=${spec%%:*}
    deadline=${spec##*:}
    case "$cell" in ""|*[!A-Z0-9_]*) echo "REFUSED: malformed leg token '$spec'"; exit 96;; esac
    case "$deadline" in ""|*[!0-9]*) echo "REFUSED: malformed deadline in '$spec'"; exit 96;; esac
    leg "$cell" "$deadline"
done

echo
echo "=== LEGS DONE. Rows that exist:"
ls -1 "$S/rows"/C-*.txt 2>/dev/null | wc -l | sed 's/^/    rows: /'
ls -1 "$S/traces"/*-all.csv 2>/dev/null | wc -l | sed 's/^/    traces: /'
du -sh "$S/traces" 2>/dev/null | sed 's/^/    trace bytes: /'

echo
echo "=== THE IDENTITY GATE, per leg (offline; no solver). P2A's gate, unmodified"
echo "    in every part that decides a field: the intersection of the two rows,"
echo "    byte equality outside the residual class, 1e-12 inside it, ABSENT never"
echo "    agreement. A cell that DIFFERS is a STOP for that cell."
python3 "$D/wrappers/gate_p2a.py" \
    --p1-rows "$D/rows" \
    --leg-dir "$S/rows" \
    --cells ACOPP57,DALLASM,STEENBRB,ACOPP118,ACOPR57,STEENBRC,STEENBRG,YORKNET,SAWPATH,BRITGAS \
    --arms C \
    --rule-outcome off \
    --out "$S/logs/gate-p2b.txt"
echo "GATE_RC=$?"
echo "CHAIN_P2B_END $(date -Is)"
