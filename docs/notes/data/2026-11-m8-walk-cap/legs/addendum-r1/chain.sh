#!/usr/bin/env bash
# M8 walk cap ADDENDUM-r1: F1, I1, I4 setup. Run as: flock /tmp/box-build.lock bash chain.sh <code sha>
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a7a2ce81cf8988de5
L=$WT/docs/notes/data/2026-11-m8-walk-cap/legs
A=$L/addendum-r1
EV=$WT/docs/notes/data/2026-11-m8-walk-cap/addendum-r1
mkdir -p $EV/suites $EV/replay
bash $L/build.sh Release rel || exit 1
bash $L/build.sh Debug dbg || exit 1
bash $L/ctest.sh rel $EV/suites/ctest-rel.log; echo F1_REL_RC=$?
bash $L/ctest.sh dbg $EV/suites/ctest-dbg.log; echo F1_DBG_RC=$?
mkdir -p $WT/build-wc/bin-r1; cp -p $WT/build-wc/rel/bench/hven_sqp_corpus $WT/build-wc/bin-r1/
sha256sum $WT/build-wc/bin-r1/hven_sqp_corpus
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 TMPDIR=$WT/build-wc/tmp
cd $WT
CELLS=$(sed -n 3p docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)
for MODE in walk ssn ipm; do
  [ -n "$(pgrep -a hven_bench_cell)" ] && { echo PGREP_BUSY; exit 3; }
  $WT/build-wc/bin-r1/hven_sqp_corpus --engine $MODE --cells "$CELLS" --csv $EV/replay/$MODE.csv > $EV/replay/run-$MODE.txt 2>&1; echo "run $MODE rc=$?"
  python3 scripts/compare_replay.py $L/rebase-92e7c5d83/$MODE.csv $EV/replay/$MODE.csv "addendum-r1-vs-rebase-$MODE" > $EV/replay/comparison-$MODE.txt 2>&1; echo "compare rc=$?"; head -1 $EV/replay/comparison-$MODE.txt
done
bash $A/train_setup.sh ${1:?code sha} m8-walk-cap-legs-r1 || exit 1
echo CHAIN_SETUP_DONE
