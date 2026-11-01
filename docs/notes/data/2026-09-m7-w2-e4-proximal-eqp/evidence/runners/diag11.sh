#!/usr/bin/env bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
B=$W/.scratch/nc2-build-debug
cmake --build $B -j 12 --target hven_qp_walk_seam_tests > $S/logs/diag11-build.log 2>&1 || { tail -40 $S/logs/diag11-build.log; exit 9; }
BIN=$(find $B/tests -name hven_qp_walk_seam_tests -type f | head -1)
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 $BIN --gtest_filter='E4Scratch.*' > $S/logs/diag11.log 2>&1
echo rc=$?
grep -E '^===|^  m=' $S/logs/diag11.log
