#!/usr/bin/env bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
B=$W/.scratch/nc2-build-debug
cmake --build $B -j 12 --target hven_sqp_tests > $S/logs/diag8-build.log 2>&1 || { tail -30 $S/logs/diag8-build.log; exit 9; }
BIN=$(find $B/tests -name hven_sqp_tests -type f | head -1)
E4_OFF=4 MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 $BIN \
  --gtest_filter='QpEngineBorder.*:QpEngineStructuralViolation.T7*:HsBattery.*:HsSweepCorpus.*' > $S/logs/diag8.log 2>&1
echo "rc=$? ok=$(grep -cE '^\[       OK \]' $S/logs/diag8.log)"
grep -E '^\[  FAILED  \] [A-Za-z]' $S/logs/diag8.log | sed 's/ (.*//' | sort -u
