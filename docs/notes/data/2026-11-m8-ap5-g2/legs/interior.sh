#!/usr/bin/env bash
# M8 AP-5 G2 (../DECLARATION.md P4): adapted from ../../2026-11-m8-ap5-g1/legs-r3/interior.sh. G2 changes no column,
# so there is no column map. Four comparisons, all scripts/compare_replay.py EXACT, on the 27 replay cells at the defaults:
#   unmoved   -- arm A (base 286fb5398 (origin/m8 the head is rebased on)) vs arm B;
#   promoted  -- arm B under the promoted baseline's own invocation vs
#                bench/baselines/2026-11-m8-ap5-g1-ipm-leg/interior_baseline.csv (G1's file, still current);
#   armed     -- arm B with HVEN_LEG_COUNT_CALLBACK=1 (a read-only counting callback, kAll) and HVEN_LEG_UNFIRED_TOKEN=1
#                (a never-requested token) vs arm B unarmed.
# usage: interior.sh <evidence-dir> <expected arm B sha256>   Run as: flock /tmp/box-build.lock bash interior.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a41fdd4ad89efc301
EV=${1:?evidence dir}
EXPECT=${2:?expected arm B sha256}
PROMOTED=$WT/bench/baselines/2026-11-m8-ap5-g1-ipm-leg/interior_baseline.csv
A=$WT/build-g2/armAbin/hven_sqp_corpus
B=$WT/build-g2/armBbin/hven_sqp_corpus
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$EV"
if [ ! -f "$B" ]; then mkdir -p "$(dirname "$B")"; cp -p "$WT/build-g2/rel/bench/hven_sqp_corpus" "$B" || exit 1; fi
GOT=$(sha256sum "$B" | cut -d' ' -f1)
sha256sum "$A" "$B" "$PROMOTED"
[ "$GOT" = "$EXPECT" ] || { echo "BINARY_SHA_MISMATCH"; exit 4; }
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
unset HVEN_LEG_PRE_B7_INFINITE_BOUND HVEN_LEG_IPM_PRE_DN3_DEFAULT HVEN_LEG_COUNT_CALLBACK HVEN_LEG_UNFIRED_TOKEN
CELLS=$(sed -n 3p docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)
BASE_CELLS=$(grep -m1 '^# invocation:' "$PROMOTED" | sed -E 's/.*--cells ([^ ]+).*/\1/')
echo "baseline cells: $BASE_CELLS"
worst=0
"$A" --engine interior --cells "$CELLS" --csv "$EV/interior-A.csv" > "$EV/interior-run-A.txt" 2>&1
rc=$?; echo "arm A rc=$rc"; [ $rc -gt $worst ] && worst=$rc
"$B" --engine interior --cells "$CELLS" --csv "$EV/interior-B.csv" > "$EV/interior-run-B.txt" 2>&1
rc=$?; echo "arm B rc=$rc"; [ $rc -gt $worst ] && worst=$rc
"$B" --engine interior --cells "$BASE_CELLS" --csv "$EV/interior-B-baseline.csv" > "$EV/interior-run-B-baseline.txt" 2>&1
rc=$?; echo "arm B (baseline invocation) rc=$rc"; [ $rc -gt $worst ] && worst=$rc
HVEN_LEG_COUNT_CALLBACK=1 HVEN_LEG_UNFIRED_TOKEN=1 "$B" --engine interior --cells "$CELLS" --csv "$EV/interior-Barmed.csv" > "$EV/interior-run-Barmed.txt" 2>&1
rc=$?; echo "arm B armed rc=$rc"; [ $rc -gt $worst ] && worst=$rc
python3 scripts/compare_replay.py "$EV/interior-A.csv" "$EV/interior-B.csv" "interior-unmoved" > "$EV/interior-comparison-unmoved.txt" 2>&1
rc=$?; tail -1 "$EV/interior-comparison-unmoved.txt"; echo "compare unmoved rc=$rc"; [ $rc -gt $worst ] && worst=$rc
python3 scripts/compare_replay.py "$PROMOTED" "$EV/interior-B-baseline.csv" "interior-promoted" > "$EV/interior-comparison-promoted.txt" 2>&1
rc=$?; tail -1 "$EV/interior-comparison-promoted.txt"; echo "compare promoted rc=$rc"; [ $rc -gt $worst ] && worst=$rc
python3 scripts/compare_replay.py "$EV/interior-B.csv" "$EV/interior-Barmed.csv" "interior-armed" > "$EV/interior-comparison-armed.txt" 2>&1
rc=$?; tail -1 "$EV/interior-comparison-armed.txt"; echo "compare armed rc=$rc"; [ $rc -gt $worst ] && worst=$rc
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
