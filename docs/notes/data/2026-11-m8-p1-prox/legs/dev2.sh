#!/usr/bin/env bash
# dev chain (not legs): build B (Release), ctest B, replay A and B (27 cells x 3 modes), PF1 probes, lane setup A + B, TRAIN A + B.
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/p1
BASE=/home/ghecht/Projects/hven/.claude/worktrees/p1-base
L=$WT/.scratch/legs; O=$WT/.scratch/p1out; mkdir -p $O
BSHA=${1:?B scratch commit}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo PGREP_BUSY; exit 3; fi
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 TMPDIR=$WT/build-k/tmp GIT_CEILING_DIRECTORIES=$WT
bash $L/build.sh $WT Release rel || { echo BUILD_FAIL; grep -m5 'error:' $WT/build-k/rel.build.log; exit 1; }
( cd $WT/build-k/rel && setsid ctest -j4 --output-on-failure > $O/ctest-rel-B.log 2>&1 ); echo "ctest rc=$?"; tail -3 $O/ctest-rel-B.log
CELLS=$(sed -n 3p $WT/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)
for MODE in walk ssn ipm; do
  $BASE/build-k/rel/bench/hven_sqp_corpus --engine $MODE --cells "$CELLS" --csv $O/A-$MODE.csv > $O/A-run-$MODE.txt 2>&1; echo "A $MODE rc=$?"
  $WT/build-k/rel/bench/hven_sqp_corpus --engine $MODE --cells "$CELLS" --csv $O/B-$MODE.csv > $O/B-run-$MODE.txt 2>&1; echo "B $MODE rc=$?"
  python3 $WT/scripts/compare_replay.py $O/A-$MODE.csv $O/B-$MODE.csv "A-vs-B-$MODE" > $O/cmp-$MODE.txt 2>&1; tail -1 $O/cmp-$MODE.txt
done
for alg in refactorize border; do
  HVEN_P1_DUMP=PF1_f7_n1000_path_corrupted_qp1.txt HVEN_P1_MAXIT=20000 HVEN_P1_ALG=$alg $WT/build-k/rel/tests/hven_qp_walk_seam_tests --gtest_filter=P1Probe.RunDump 2>&1 | grep -a '\[ p1 \]' | cut -c1-200
done
bash $L/train_setup.sh A 3edf5ede4ef2cd95f1fa5d94ae19d40e018ae1d3 || exit 1
bash $L/train_setup.sh B $BSHA || exit 1
for ARM in A B; do
  bash $L/train_enum.sh $O/train-$ARM $WT/build-k/bin/$ARM/hven_bench_cell $(sha256sum $WT/build-k/bin/$ARM/hven_bench_cell | cut -d' ' -f1)
done
python3 $L/train_check.py $O/train-B $WT/docs/notes/data/2026-11-m8-dn3 $O/train-A > $O/train_check-dev.txt 2>&1; echo "check rc=$?"
grep -E '^(SUMMARY|MOVER)' $O/train_check-dev.txt | head -60
echo "FOREGROUND_END $(date -Is)"
