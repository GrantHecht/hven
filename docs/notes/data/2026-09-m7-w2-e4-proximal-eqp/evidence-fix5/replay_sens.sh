#!/usr/bin/env bash
set -o pipefail
S=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9/.scratch/e4-fix5
R=$S/replay-build-ff02789b46a7/dump_replay
for arm in "--algebra border --e2 1" "--algebra refactorize --e2 1" "--algebra border --e2 0" "--algebra refactorize --e2 0"; do
  echo "----- E4 arm: $arm -----"
  env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 $R $arm $S/dumps/ACOPR118_e4/qp5.txt \
    | sed 's|.*/qp5.txt|qp5|' | cut -c1-200
done
for arm in "--algebra border --e2 1" "--algebra refactorize --e2 1"; do
  echo "----- RT8 arm: $arm -----"
  env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 $S/replay-build-85dc01ac9f53/dump_replay $arm $S/dumps/ACOPR118_e4/qp5.txt \
    | sed 's|.*/qp5.txt|qp5|' | cut -c1-200
done
