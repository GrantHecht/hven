#!/usr/bin/env bash
# dev experiment (not a leg): build B, PF1 probes under env variants.
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/p1
L=$WT/.scratch/legs
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo PGREP_BUSY; exit 3; fi
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 TMPDIR=$WT/build-k/tmp GIT_CEILING_DIRECTORIES=$WT
bash $L/build.sh $WT Release rel hven_qp_walk_seam_tests || { echo BUILD_FAIL; exit 1; }
for v in "trig-norung:HVEN_P1_NORUNG=1" "global:HVEN_P1_GLOBAL=1" "global-norung:HVEN_P1_GLOBAL=1 HVEN_P1_NORUNG=1"; do
  n=${v%%:*}; e=${v#*:}
  for alg in refactorize border; do
    env $e HVEN_P1_DUMP=PF1_f7_n1000_path_corrupted_qp1.txt HVEN_P1_MAXIT=20000 HVEN_P1_ALG=$alg timeout 900 $WT/build-k/rel/tests/hven_qp_walk_seam_tests --gtest_filter=P1Probe.RunDump 2>&1 | grep -a '\[ p1 \]' | sed "s/^/$n $alg /" | cut -c1-220
  done
done
echo "FOREGROUND_END $(date -Is)"
