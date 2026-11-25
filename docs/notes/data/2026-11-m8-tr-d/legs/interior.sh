#!/usr/bin/env bash
# engine, scripts/compare_replay.py EXACT: unmoved (arm A vs arm B, the 27 replay cells), promoted (arm B under the promoted
# baseline's own invocation vs bench/baselines/2026-11-m8-ap5-g1-ipm-leg/interior_baseline.csv), armed (counting callback +
# unfired token vs arm B unarmed).
# usage: interior.sh <evidence-dir> <expected arm B sha256>   Run as: flock /tmp/box-build.lock bash interior.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/tr-d
EV=${1:?evidence dir}
EXPECT=${2:?expected arm B sha256}
PROMOTED=$WT/bench/baselines/2026-11-m8-ap5-g1-ipm-leg/interior_baseline.csv
A=$WT/.scratch/build-trd/armAbin/hven_sqp_corpus
B=$WT/.scratch/build-trd/armBbin/hven_sqp_corpus
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$EV"
if [ ! -f "$B" ]; then mkdir -p "$(dirname "$B")"; cp -p "$WT/.scratch/build-trd/rel/bench/hven_sqp_corpus" "$B" || exit 1; fi
GOT=$(sha256sum "$B" | cut -d' ' -f1)
sha256sum "$A" "$B" "$PROMOTED"
[ "$GOT" = "$EXPECT" ] || { echo "BINARY_SHA_MISMATCH"; exit 4; }
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 GIT_CEILING_DIRECTORIES=$WT
unset HVEN_LEG_PRE_B7_INFINITE_BOUND HVEN_LEG_IPM_PRE_DN3_DEFAULT HVEN_LEG_COUNT_CALLBACK HVEN_LEG_UNFIRED_TOKEN
CELLS=$(sed -n 3p docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)
BASE_CELLS=$(grep -m1 '^# invocation:' "$PROMOTED" | sed -E 's/.*--cells ([^ ]+).*/\1/')
echo "baseline cells: $BASE_CELLS"
worst=0
"$A" --engine interior --cells "$CELLS" --csv "$EV/trd-interior-A.csv" > "$EV/trd-interior-run-A.txt" 2>&1
rc=$?; echo "arm A rc=$rc"; [ $rc -gt $worst ] && worst=$rc
"$B" --engine interior --cells "$CELLS" --csv "$EV/trd-interior-B.csv" > "$EV/trd-interior-run-B.txt" 2>&1
rc=$?; echo "arm B rc=$rc"; [ $rc -gt $worst ] && worst=$rc
"$B" --engine interior --cells "$BASE_CELLS" --csv "$EV/trd-interior-B-baseline.csv" > "$EV/trd-interior-run-B-baseline.txt" 2>&1
rc=$?; echo "arm B (baseline invocation) rc=$rc"; [ $rc -gt $worst ] && worst=$rc
HVEN_LEG_COUNT_CALLBACK=1 HVEN_LEG_UNFIRED_TOKEN=1 "$B" --engine interior --cells "$CELLS" --csv "$EV/trd-interior-Barmed.csv" > "$EV/trd-interior-run-Barmed.txt" 2>&1
rc=$?; echo "arm B armed rc=$rc"; [ $rc -gt $worst ] && worst=$rc
python3 scripts/compare_replay.py "$EV/trd-interior-A.csv" "$EV/trd-interior-B.csv" "interior-unmoved" > "$EV/trd-interior-comparison-unmoved.txt" 2>&1
rc=$?; tail -1 "$EV/trd-interior-comparison-unmoved.txt"; echo "compare unmoved rc=$rc"; [ $rc -gt $worst ] && worst=$rc
python3 scripts/compare_replay.py "$PROMOTED" "$EV/trd-interior-B-baseline.csv" "interior-promoted" > "$EV/trd-interior-comparison-promoted.txt" 2>&1
rc=$?; tail -1 "$EV/trd-interior-comparison-promoted.txt"; echo "compare promoted rc=$rc"; [ $rc -gt $worst ] && worst=$rc
python3 scripts/compare_replay.py "$EV/trd-interior-B.csv" "$EV/trd-interior-Barmed.csv" "interior-armed" > "$EV/trd-interior-comparison-armed.txt" 2>&1
rc=$?; tail -1 "$EV/trd-interior-comparison-armed.txt"; echo "compare armed rc=$rc"; [ $rc -gt $worst ] && worst=$rc
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
