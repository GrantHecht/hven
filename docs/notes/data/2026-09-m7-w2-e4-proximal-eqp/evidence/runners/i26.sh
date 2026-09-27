#!/usr/bin/env bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
F=$(paste -sd: $S/logs2/rel-i24-failures.txt)
B=$(find $W/.scratch/nc2-build-release/tests -name hven_sqp_tests -type f | head -1)
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 $B --gtest_filter="$F" > $S/logs2/i26.log 2>&1
echo rc=$?
