#!/usr/bin/env bash
set -o pipefail
S=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9/.scratch/e4-fix5
cmake --build $S/build-rel --parallel 16 > $S/logs/build-hven4.log 2>&1 || { tail -30 $S/logs/build-hven4.log; exit 82; }
cmake --install $S/build-rel > $S/logs/install-hven4.log 2>&1 || exit 83
cmake --build $S/replay-build-diag > $S/logs/build-replay-diag3.log 2>&1 || exit 2
for arm in 1 0; do
  echo "===== e2=$arm ====="
  env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 HVEN_F5_TRACE=1 taskset -c 2 \
    $S/replay-build-diag/dump_replay --e2 $arm --algebra border $S/dumps/ACOPR118_e4/qp5.txt 2>&1 \
    | grep -E '^F5' | head -22 | cut -c1-185
done
