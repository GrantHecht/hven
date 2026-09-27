#!/usr/bin/env bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
B=$W/.scratch/nc2-build-debug
cmake --build $B -j 12 --target hven_sqp_tests > $S/logs/diag1-build.log 2>&1 || { tail -30 $S/logs/diag1-build.log; exit 9; }
BIN=$(find $B/tests -name hven_sqp_tests -type f | head -1)
for m in 0 1 2 4 8 15; do
  E4_OFF=$m MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 $BIN \
    --gtest_filter='HsBattery.RefactorizeMode:HsBattery.BorderMode:QpEngineBorder.BorderModeMatchesRefactorizeMode:EqpSolve.*:QpEngineStructuralViolation.T7ACertifiedInfeasibleExitRETURNSTheRefinedPointInBothAlgebras' \
    > $S/logs/diag1-m$m.log 2>&1
  echo "--- E4_OFF=$m rc=$? : $(grep -cE '^\[       OK \]' $S/logs/diag1-m$m.log) ok"
  grep -E '^\[  FAILED  \] [A-Za-z]' $S/logs/diag1-m$m.log | sed 's/ (.*//' | sort -u | sed 's/^/    /'
  grep -oE 'HS[0-9]+ \((refactorize|border) mode\)' $S/logs/diag1-m$m.log | sort -u | tr '\n' ' '; echo
done
