#!/usr/bin/env bash
# M7 W2 M3c RULE-P2A section 4 -- the six legs, then section 5's identity gate.
#
# Two cells x three arms, DEFAULT options throughout (`--tr-init` is never
# passed, so SqpOptions::tr_init is not assigned at all). One solver process at a
# time, the box lock taken per leg and released between legs, 600 s solver
# deadline inside `timeout 900`. A leg the outer stop kills is CENSORED: its row
# and its trace are DELETED and are never reconstructed.
#
# The gate at the end runs NO solver. Nothing here changes a file under src/ or
# include/, and the harness tree is READ-ONLY.
set -o pipefail

ROOT=/home/ghecht/Projects/hven
BENCH=/home/ghecht/Projects/hven-bench
D="$ROOT/docs/notes/data/2026-09-m7-w2-m3c-default-census"
S="$ROOT/.scratch/m7w2m3c/p2a"
LOCK=/tmp/box-build.lock
DEADLINE=600

mkdir -p "$S/rows" "$S/traces" "$S/logs"

for p in "$S/armA/m3c_p2a_probe_armA" "$S/armB/m3c_p2a_probe_armB" \
         "$D/RULE-P2A.md" "$D/rows" "$D/wrappers/gate_p2a.py" \
         "$BENCH/.scratch/h3/decoded"; do
    [ -e "$p" ] || { echo "REFUSED: missing $p"; exit 90; }
done

echo "CHAIN_P2A_START $(date -Is)"
echo "=== HEAD $(git -C "$ROOT" rev-parse --short HEAD)  dirty-files=$(git -C "$ROOT" status --porcelain | wc -l)"
cat "$S/logs/binaries-p2a.txt"

# arm:cell and NOTHING ELSE on the token (the pass-2 word-split defect's guard).
LEGS="
A:ACOPP57
A:DALLASM
B:ACOPP57
B:DALLASM
C:ACOPP57
C:DALLASM
"

leg() {
    arm=$1; cell=$2
    echo
    echo "PGREP_BEFORE_LOCK $arm-$cell: $(pgrep -a -f 'hven_bench_cell|m3c_probe|m3c_p2_probe|m3c_p2a_probe|hven_sqp_corpus' | grep -v 'eval \|pgrep' | head -3)"
    flock "$LOCK" bash -c '
      set -o pipefail
      arm='"$arm"'; cell='"$cell"'; deadline='"$DEADLINE"'
      S='"$S"'; BENCH='"$BENCH"'
      echo "FOREGROUND_START leg-$arm-$cell $(date -Is)"
      if pgrep -a -f "hven_bench_cell|m3c_probe|m3c_p2_probe|m3c_p2a_probe|hven_sqp_corpus" | grep -v "eval \|pgrep"; then
        echo "FOREIGN CELL BINARY UNDER MY LOCK -- SKIPPING $arm-$cell"
        echo "FOREGROUND_END leg-$arm-$cell $(date -Is) rc=99"
        exit 99
      fi
      echo "PGREP_EMPTY leg-$arm-$cell: no foreign cell binary under my lock"
      case "$arm" in
        A) PROBE="$S/armA/m3c_p2a_probe_armA"; ARMING="--trace off" ;;
        B) PROBE="$S/armB/m3c_p2a_probe_armB"; ARMING="--trace off" ;;
        C) PROBE="$S/armB/m3c_p2a_probe_armB"
           ARMING="--trace on --trace-out $S/traces/C-$cell.csv --capture-solve -1 --max-records 2000000" ;;
        *) echo "REFUSED: unknown arm $arm"; exit 96 ;;
      esac
      cd "$BENCH" || exit 93
      t0=$(date +%s.%N)
      # shellcheck disable=SC2086
      MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
        timeout 900 "$PROBE" \
          --problem "$cell" \
          --deadline "$deadline" \
          --stop-after-first-qp off \
          --ledger on \
          $ARMING \
          > "$S/rows/$arm-$cell.txt" 2> "$S/rows/$arm-$cell.stderr"
      rc=$?
      t1=$(date +%s.%N)
      echo "LEG $arm-$cell   deadline=$deadline   rc=$rc   wall_s=$(echo "$t1 - $t0" | bc) (INFORMATIONAL)"
      if [ $rc -ne 0 ]; then
        echo "CENSORED $arm-$cell: rc=$rc -- the row and the trace are DELETED and are never reconstructed"
        head -3 "$S/rows/$arm-$cell.stderr" 2>/dev/null
        rm -f "$S/rows/$arm-$cell.txt" "$S/traces/C-$cell.csv"
      else
        for k in status major_iters qp_minor_iters factorizations f stationarity \
                 deadline_fired tr_init_touched trace_armed summary_solves_seen; do
          printf "    %s\n" "$(grep -m1 "^$k=" "$S/rows/$arm-$cell.txt" || echo "$k=ABSENT")"
        done
      fi
      echo "FOREGROUND_END leg-$arm-$cell $(date -Is) rc=$rc"
      exit $rc
    '
    echo "WRAPPER_RC leg-$arm-$cell rc=$?"
}

for spec in $LEGS; do
    [ -z "$spec" ] && continue
    arm=${spec%%:*}
    cell=${spec##*:}
    case "$arm" in A|B|C) ;; *) echo "REFUSED: malformed arm in '$spec'"; exit 96;; esac
    case "$cell" in ""|*[!A-Z0-9_]*) echo "REFUSED: malformed cell in '$spec'"; exit 96;; esac
    leg "$arm" "$cell"
done

echo
echo "=== LEGS DONE. Rows that exist:"
ls -1 "$S/rows"/*.txt 2>/dev/null | grep -v stderr | sed 's/^/    /'

echo
echo "=== RULE-P2A section 5's IDENTITY GATE (offline; no solver)"
python3 "$D/wrappers/gate_p2a.py" \
    --p1-rows "$D/rows" \
    --leg-dir "$S/rows" \
    --cells ACOPP57,DALLASM \
    --arms A,B,C \
    --out "$S/logs/gate-p2a.txt"
echo "GATE_RC=$?"
echo "CHAIN_P2A_END $(date -Is)"
