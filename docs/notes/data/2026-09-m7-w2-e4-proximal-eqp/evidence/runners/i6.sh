#!/usr/bin/env bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
BIN=$(find $W/.scratch/nc2-build-release/tests -name hven_qp_walk_seam_tests -type f | head -1)
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 $BIN --gtest_filter="$1" > "$2" 2>&1
echo rc=$?
