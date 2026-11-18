#!/usr/bin/env bash
# dev (not a leg): Release ctest with the prototype arm on; replay 27 cells x walk/ssn/ipm arm off and on.
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/p1
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo PGREP_BUSY; exit 3; fi
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 TMPDIR=$WT/build-k/tmp GIT_CEILING_DIRECTORIES=$WT
O=$WT/.scratch/p1out; mkdir -p $O
cd $WT/build-k/rel && HVEN_P1_ARM=1 HVEN_P1_NORUNG=1 setsid ctest -j4 --output-on-failure > $O/ctest-rel-arm.log 2>&1; echo "ctest rc=$?"; tail -3 $O/ctest-rel-arm.log
cd $WT
CELLS=$(sed -n 3p docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)
for MODE in walk ssn ipm; do
  build-k/rel/bench/hven_sqp_corpus --engine $MODE --cells "$CELLS" --csv $O/off-$MODE.csv > $O/off-run-$MODE.txt 2>&1; echo "off $MODE rc=$?"
  HVEN_P1_ARM=1 HVEN_P1_NORUNG=1 build-k/rel/bench/hven_sqp_corpus --engine $MODE --cells "$CELLS" --csv $O/arm-$MODE.csv > $O/arm-run-$MODE.txt 2>&1; echo "arm $MODE rc=$?"
done
echo "FOREGROUND_END $(date -Is)"
