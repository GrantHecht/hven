#!/usr/bin/env bash
# M8 walk cap (../DECLARATION.md, I3): ../../2026-11-m8-dn3/r5/legs/interior.sh re-cut. The interior-point engine does
# not reach the QP walk or the SQP driver, so the fix must not move it: the frozen corpus binary runs the interior leg
# at the default twice (the 27-cell invocation and the promoted baseline's invocation), and
#   I3-A  default 27-cell run  vs ../../2026-11-m8-dn3/r5/interior/interior-default.csv (DN-3 r5, the base)  0 differences
#   I3-B  default baseline run vs bench/baselines/2026-11-m8-dn3-flip-ipm-leg/interior_baseline.csv          0 differences
# compare_replay.py byte-exact throughout (no --residual-gate).
# usage: interior.sh <evidence-dir> <corpus binary sha256>   Run as: flock /tmp/box-build.lock bash interior.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a7a2ce81cf8988de5
EV=${1:?evidence dir}
EXPECT=${2:?corpus binary sha256}
PROMOTED=$WT/bench/baselines/2026-11-m8-dn3-flip-ipm-leg/interior_baseline.csv
R5RUN=$WT/docs/notes/data/2026-11-m8-dn3/r5/interior/interior-default.csv
B=$WT/build-wc/bin/hven_sqp_corpus
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$EV"
GOT=$(sha256sum "$B" | cut -d' ' -f1)
sha256sum "$B" "$PROMOTED" "$R5RUN"
[ "$GOT" = "$EXPECT" ] || { echo "BINARY_SHA_MISMATCH"; exit 4; }
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 TMPDIR=$WT/build-wc/tmp
unset HVEN_LEG_PRE_B7_INFINITE_BOUND HVEN_LEG_IPM_PRE_D1_DEFAULTS HVEN_LEG_IPM_PRE_DN3_DEFAULT
CELLS=$(sed -n 3p docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)
BASE_CELLS=$(grep -m1 '^# invocation:' "$PROMOTED" | sed -E 's/.*--cells ([^ ]+).*/\1/')
echo "baseline cells: $BASE_CELLS"
worst=0
run() { local tag=$1 cells=$2
  "$B" --engine interior --cells "$cells" --csv "$EV/interior-$tag.csv" > "$EV/interior-run-$tag.txt" 2>&1
  local rc=$?; echo "run $tag rc=$rc"; [ $rc -gt $worst ] && worst=$rc; }
cmpr() { local name=$1 ref=$2 new=$3
  python3 scripts/compare_replay.py "$ref" "$new" "$name" > "$EV/comparison-$name.txt" 2>&1; local rc=$?
  head -1 "$EV/comparison-$name.txt"; echo "compare $name rc=$rc"; return $rc; }
run default "$CELLS"
run default-baseline "$BASE_CELLS"
cmpr I3-A "$R5RUN" "$EV/interior-default.csv"; rc=$?; [ $rc -gt $worst ] && worst=$rc
cmpr I3-B "$PROMOTED" "$EV/interior-default-baseline.csv"; rc=$?; [ $rc -gt $worst ] && worst=$rc
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
