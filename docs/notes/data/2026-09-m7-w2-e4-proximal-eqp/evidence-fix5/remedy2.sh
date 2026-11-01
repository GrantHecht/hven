#!/usr/bin/env bash
set -o pipefail
S=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9/.scratch/e4-fix5
env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 HVEN_F5_TRACE=1 HVEN_F5_UNGATE=1 taskset -c 2 \
  $S/replay-build-diag/dump_replay --e2 1 --algebra border $S/dumps/ACOPR118_e4/qp5.txt 2>&1 \
  | grep -E '^F5' | cut -c1-200
