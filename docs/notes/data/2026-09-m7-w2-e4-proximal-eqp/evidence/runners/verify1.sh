#!/usr/bin/env bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 $(find $W/.scratch/nc2-build-release/tests -name hven_sqp_tests -type f | head -1) \
  --gtest_filter='HsBattery.CrashBasisIsANullResultOnTheBatteryWithTwoBoundSeededExceptions:HsBattery.RefinementStepCountersOnBorderRepro:ScaleF7.WideWindowWalkIsChurnNotDegeneracy:F7ColdScaleSmoke.EmptyWindowColdSolveAtTenThousandVariables:PredictorRatioTest.InheritsTheTrueActivitySetAcrossAnActivationThreshold:IpqpAcceptanceCensus.AnArmedRunIsNeverMistakenForAStallAcrossTheHsBattery:QpEngineStructuralViolation.T7ACertifiedInfeasibleExitRETURNSTheRefinedPointInBothAlgebras:WarmStart.BudgetModeOffMatchesMaxIterExactly' \
  > $S/logs/verify1.log 2>&1
echo "rc=$? ok=$(grep -cE '^\[       OK \]' $S/logs/verify1.log) failed=$(grep -cE '^\[  FAILED  \] [A-Za-z]' $S/logs/verify1.log)"
grep -E '^\[  FAILED  \] [A-Za-z]' $S/logs/verify1.log | sed 's/ (.*//' | sort -u
