#!/usr/bin/env bash
# M8 DN-3 r5 (../../DECLARATION-r5.md, I3c): ../../r4/legs/interior.sh re-cut after r4's stop. The frozen corpus binary
# of the r5 code head runs the interior leg four times: the 27-cell invocation and the promoted baseline's invocation,
# each at the default (kAuto) and with the pre-flip dense pin HVEN_LEG_IPM_PRE_DN3_DEFAULT=1 (kNever). Then:
#   I3c-N1  pinned 27-cell run   vs ../../interior-r2/interior-B.csv (the implementation head 96ac1a27d)  0 differences
#   I3c-N2  pinned baseline run  vs bench/baselines/2026-11-m8-b7-ipm-leg/interior_baseline.csv          0 differences
#   I3c-A1  default 27-cell run  vs ../../r4/interior/interior-flip.csv (r4's recorded flip run)          0 differences
#   I3c-A2  default 27-cell run  vs ../../interior-r2/interior-B.csv: every difference on a declared mover cell
#           (hs071_x1_fixed, infeas2_spike, infeas2_stationary), mover_check.py MOVERS_AS_DECLARED
#   I3c-A3  default baseline run vs ../../r4/interior/interior-flip-baseline.csv                         0 differences
# compare_replay.py byte-exact throughout (no --residual-gate).
# usage: interior.sh <evidence-dir> <corpus binary sha256>   Run as: flock /tmp/box-build.lock bash interior.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a739270f237b3e30c
D3=$WT/docs/notes/data/2026-11-m8-dn3
EV=${1:?evidence dir}
EXPECT=${2:?corpus binary sha256}
PROMOTED=$WT/bench/baselines/2026-11-m8-b7-ipm-leg/interior_baseline.csv
HEADRUN=$D3/interior-r2/interior-B.csv
R4RUN=$D3/r4/interior/interior-flip.csv
R4BASE=$D3/r4/interior/interior-flip-baseline.csv
MOVERS=hs071_x1_fixed,infeas2_spike,infeas2_stationary
B=$WT/build-flip/flipbin/hven_sqp_corpus
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$EV"
GOT=$(sha256sum "$B" | cut -d' ' -f1)
sha256sum "$B" "$PROMOTED" "$HEADRUN" "$R4RUN" "$R4BASE"
[ "$GOT" = "$EXPECT" ] || { echo "BINARY_SHA_MISMATCH"; exit 4; }
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 TMPDIR=$WT/build-flip/tmp
unset HVEN_LEG_PRE_B7_INFINITE_BOUND HVEN_LEG_IPM_PRE_D1_DEFAULTS HVEN_LEG_IPM_PRE_DN3_DEFAULT
CELLS=$(sed -n 3p docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)
BASE_CELLS=$(grep -m1 '^# invocation:' "$PROMOTED" | sed -E 's/.*--cells ([^ ]+).*/\1/')
echo "baseline cells: $BASE_CELLS"
worst=0
run() { local tag=$1 cells=$2 pin=$3
  if [ "$pin" = 1 ]; then HVEN_LEG_IPM_PRE_DN3_DEFAULT=1 "$B" --engine interior --cells "$cells" --csv "$EV/interior-$tag.csv" > "$EV/interior-run-$tag.txt" 2>&1
  else "$B" --engine interior --cells "$cells" --csv "$EV/interior-$tag.csv" > "$EV/interior-run-$tag.txt" 2>&1; fi
  local rc=$?; echo "run $tag rc=$rc"; grep -m1 'pre_dn3_dense_pin' "$EV/interior-$tag.csv" | grep -oE 'pre_dn3_dense_pin=[a-z]+'
  [ $rc -gt $worst ] && worst=$rc; }
cmpr() { local name=$1 ref=$2 new=$3
  python3 scripts/compare_replay.py "$ref" "$new" "$name" > "$EV/comparison-$name.txt" 2>&1; local rc=$?
  head -1 "$EV/comparison-$name.txt"; echo "compare $name rc=$rc"; return $rc; }
run default "$CELLS" 0
run default-baseline "$BASE_CELLS" 0
run pinned "$CELLS" 1
run pinned-baseline "$BASE_CELLS" 1
cmpr I3c-N1 "$HEADRUN" "$EV/interior-pinned.csv"; rc=$?; [ $rc -gt $worst ] && worst=$rc
cmpr I3c-N2 "$PROMOTED" "$EV/interior-pinned-baseline.csv"; rc=$?; [ $rc -gt $worst ] && worst=$rc
cmpr I3c-A1 "$R4RUN" "$EV/interior-default.csv"; rc=$?; [ $rc -gt $worst ] && worst=$rc
cmpr I3c-A2 "$HEADRUN" "$EV/interior-default.csv"
python3 "$D3/r5/legs/mover_check.py" "$EV/comparison-I3c-A2.txt" "$MOVERS" > "$EV/mover-check-I3c-A2.txt" 2>&1; rc=$?
cat "$EV/mover-check-I3c-A2.txt"; echo "mover_check rc=$rc"; [ $rc -gt $worst ] && worst=$rc
cmpr I3c-A3 "$R4BASE" "$EV/interior-default-baseline.csv"; rc=$?; [ $rc -gt $worst ] && worst=$rc
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
