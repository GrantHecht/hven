#!/usr/bin/env bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
B=$W/.scratch/nc2-build-debug
cmake --build $B -j 12 --target hven_sqp_tests > $S/logs/diag10-build.log 2>&1 || { tail -30 $S/logs/diag10-build.log; exit 9; }
BIN=$(find $B/tests -name hven_sqp_tests -type f | head -1)
for m in 0 1 8 9 15; do
E4_OFF=$m MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 $BIN \
  --gtest_filter='QpEngineBorder.BorderModeMatchesRefactorizeMode:QpEngineBorder.RebuildUnderTinySchurCapMatchesRefactorize:QpEngineStructuralViolation.T7ACertifiedInfeasibleExitRETURNSTheRefinedPointInBothAlgebras:HsBattery.CrashBasisIsANullResultOnTheBatteryWithTwoBoundSeededExceptions:HsBattery.RefinementStepCountersOnBorderRepro' \
  > $S/logs/diag10-m$m.log 2>&1
echo "=== E4_OFF=$m rc=$? ok=$(grep -cE '^\[       OK \]' $S/logs/diag10-m$m.log)"
grep -E '^\[  FAILED  \] [A-Za-z]' $S/logs/diag10-m$m.log | sed 's/ (.*//' | sort -u | sed 's/^/   /'
done
