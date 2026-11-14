#!/usr/bin/env bash
# M8 DN-3 r4 (../../DECLARATION-r4.md, I3c): ../../legs/interior.sh re-pointed. No arm A is rebuilt: "unmoved" compares
# the flip binary's interior run with r2's committed arm-B run of the implementation head 96ac1a27d
# (../../interior-r2/interior-B.csv, the same 27 cells and invocation; src/include/tests are byte-identical between
# 96ac1a27d and this branch's base b09f1bbc0); "promoted" compares the flip binary's run of the baseline's own invocation
# with bench/baselines/2026-11-m8-b7-ipm-leg/interior_baseline.csv, exactly as r2's P3.
# usage: interior.sh <evidence-dir> <corpus binary sha256>   Run as: flock /tmp/box-build.lock bash interior.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a739270f237b3e30c
EV=${1:?evidence dir}
EXPECT=${2:?corpus binary sha256}
PROMOTED=$WT/bench/baselines/2026-11-m8-b7-ipm-leg/interior_baseline.csv
HEADRUN=$WT/docs/notes/data/2026-11-m8-dn3/interior-r2/interior-B.csv
B=$WT/build-flip/flipbin/hven_sqp_corpus
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$EV"
GOT=$(sha256sum "$B" | cut -d' ' -f1)
sha256sum "$B" "$PROMOTED" "$HEADRUN"
[ "$GOT" = "$EXPECT" ] || { echo "BINARY_SHA_MISMATCH"; exit 4; }
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 TMPDIR=$WT/build-flip/tmp
unset HVEN_LEG_PRE_B7_INFINITE_BOUND
CELLS=$(sed -n 3p docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)
BASE_CELLS=$(grep -m1 '^# invocation:' "$PROMOTED" | sed -E 's/.*--cells ([^ ]+).*/\1/')
echo "baseline cells: $BASE_CELLS"
worst=0
"$B" --engine interior --cells "$CELLS" --csv "$EV/interior-flip.csv" > "$EV/interior-run-flip.txt" 2>&1
rc=$?; echo "flip rc=$rc"; [ $rc -gt $worst ] && worst=$rc
"$B" --engine interior --cells "$BASE_CELLS" --csv "$EV/interior-flip-baseline.csv" > "$EV/interior-run-flip-baseline.txt" 2>&1
rc=$?; echo "flip (baseline invocation) rc=$rc"; [ $rc -gt $worst ] && worst=$rc
python3 scripts/compare_replay.py "$HEADRUN" "$EV/interior-flip.csv" "interior-unmoved" > "$EV/interior-comparison-unmoved.txt" 2>&1
rc=$?; tail -1 "$EV/interior-comparison-unmoved.txt"; echo "compare unmoved rc=$rc"; [ $rc -gt $worst ] && worst=$rc
python3 scripts/compare_replay.py "$PROMOTED" "$EV/interior-flip-baseline.csv" "interior-promoted" > "$EV/interior-comparison-promoted.txt" 2>&1
rc=$?; tail -1 "$EV/interior-comparison-promoted.txt"; echo "compare promoted rc=$rc"; [ $rc -gt $worst ] && worst=$rc
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
