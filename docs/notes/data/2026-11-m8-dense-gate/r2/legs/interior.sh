#!/usr/bin/env bash
# M8 dense gate (../DECLARATION-r2.md, I3): the interior leg at the default (kAuto), DN-3 r5's two invocations:
#   I3-1  the 27-cell invocation       vs ../../2026-11-m8-ap5-g1/interior-r3/interior-B.csv (ap5-g1 r3 arm B)     0 differences
#   I3-2  the baseline invocation      vs bench/baselines/2026-11-m8-ap5-g1-ipm-leg/interior_baseline.csv 0 differences
# compare_replay.py byte-exact (no --residual-gate).
# usage: interior.sh <evidence-dir> <corpus binary sha256>   Run as: flock /tmp/box-build.lock bash interior.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a44d4824b0beb0ea7
EV=${1:?evidence dir}
EXPECT=${2:?corpus binary sha256}
PROMOTED=$WT/bench/baselines/2026-11-m8-ap5-g1-ipm-leg/interior_baseline.csv
R5RUN=$WT/docs/notes/data/2026-11-m8-ap5-g1/interior-r3/interior-B.csv
B=$WT/build-dg/dgbin-r2/hven_sqp_corpus
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$EV"
GOT=$(sha256sum "$B" | cut -d' ' -f1)
sha256sum "$B" "$PROMOTED" "$R5RUN"
[ "$GOT" = "$EXPECT" ] || { echo "BINARY_SHA_MISMATCH"; exit 4; }
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 TMPDIR=$WT/build-dg/tmp
unset HVEN_LEG_PRE_B7_INFINITE_BOUND HVEN_LEG_IPM_PRE_D1_DEFAULTS HVEN_LEG_IPM_PRE_DN3_DEFAULT
CELLS=$(sed -n 3p docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)
BASE_CELLS=$(grep -m1 '^# invocation:' "$PROMOTED" | sed -E 's/.*--cells ([^ ]+).*/\1/')
echo "baseline cells: $BASE_CELLS"
worst=0
"$B" --engine interior --cells "$CELLS" --csv "$EV/interior-default.csv" > "$EV/interior-run-default.txt" 2>&1
rc=$?; echo "run default rc=$rc"; [ $rc -gt $worst ] && worst=$rc
"$B" --engine interior --cells "$BASE_CELLS" --csv "$EV/interior-default-baseline.csv" > "$EV/interior-run-default-baseline.txt" 2>&1
rc=$?; echo "run default-baseline rc=$rc"; [ $rc -gt $worst ] && worst=$rc
cmpr() { local name=$1 ref=$2 new=$3
  python3 scripts/compare_replay.py "$ref" "$new" "$name" > "$EV/comparison-$name.txt" 2>&1; local rc=$?
  head -1 "$EV/comparison-$name.txt"; echo "compare $name rc=$rc"; return $rc; }
cmpr I3-1 "$R5RUN" "$EV/interior-default.csv"; rc=$?; [ $rc -gt $worst ] && worst=$rc
cmpr I3-2 "$PROMOTED" "$EV/interior-default-baseline.csv"; rc=$?; [ $rc -gt $worst ] && worst=$rc
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
