#!/usr/bin/env bash
# dev (not a leg): build B, ctest B, replay walk B (A rows from dev2), PF1 probes.
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/p1
L=$WT/.scratch/legs; O=$WT/.scratch/p1out/${1:?tag}; mkdir -p $O
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo PGREP_BUSY; exit 3; fi
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 TMPDIR=$WT/build-k/tmp GIT_CEILING_DIRECTORIES=$WT
bash $L/build.sh /home/ghecht/Projects/hven/.claude/worktrees/p1-base Release rel || { echo BASE_BUILD_FAIL; exit 1; }
bash $L/build.sh $WT Release rel || { echo BUILD_FAIL; grep -m5 'error:' $WT/build-k/rel.build.log; exit 1; }
( cd $WT/build-k/rel && setsid ctest -j4 --output-on-failure > $O/ctest-rel-B.log 2>&1 ); echo "ctest rc=$?"; grep 'tests passed' $O/ctest-rel-B.log
for alg in refactorize border; do
  HVEN_P1_DUMP=PF1_f7_n1000_path_corrupted_qp1.txt HVEN_P1_MAXIT=20000 HVEN_P1_ALG=$alg timeout 900 $WT/build-k/rel/tests/hven_qp_walk_seam_tests --gtest_filter=P1Probe.RunDump 2>&1 | grep -a '\[ p1 \]' | cut -c1-200
done
CELLS=$(sed -n 3p $WT/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)
for MODE in walk ssn ipm; do
  timeout 1200 /home/ghecht/Projects/hven/.claude/worktrees/p1-base/build-k/rel/bench/hven_sqp_corpus --engine $MODE --cells "$CELLS" --csv $O/A-$MODE.csv > $O/A-run-$MODE.txt 2>&1; echo "A $MODE rc=$?"
  timeout 1200 $WT/build-k/rel/bench/hven_sqp_corpus --engine $MODE --cells "$CELLS" --csv $O/B-$MODE.csv > $O/B-run-$MODE.txt 2>&1; echo "B $MODE rc=$?"
done
echo "FOREGROUND_END $(date -Is)"
