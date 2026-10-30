#!/usr/bin/env bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
B=$W/.scratch/nc2-build-debug
cmake --build $B -j 12 > $S/logs/diag4-build.log 2>&1 || { tail -30 $S/logs/diag4-build.log; exit 9; }
BIN=$(find $B/tests -name hven_sqp_tests -type f | head -1)
for m in 0 2; do
E4_OFF=$m E4_DIAG=1 MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 $BIN \
  --gtest_filter='HsBattery.*:HsSweepCorpus.*:EqpSolve.*:QpEngineBorder.*:QpEngineStructuralViolation.T7*' \
  > $S/logs/diag4-m$m.log 2>&1
echo "=== E4_OFF=$m rc=$? ok=$(grep -cE '^\[       OK \]' $S/logs/diag4-m$m.log)"
grep -E '^\[  FAILED  \] [A-Za-z]' $S/logs/diag4-m$m.log | sed 's/ (.*//' | sort -u | sed 's/^/    /'
grep E4DIAG $S/logs/diag4-m$m.log | awk '{print $2}' | sort | uniq -c
done
