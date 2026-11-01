#!/usr/bin/env bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
run() { # tag binary filter
  MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 $(find $W/.scratch/nc2-build-release/tests -name $2 -type f | head -1) \
    --gtest_filter="$3" > $S/logs/h-$1.log 2>&1
  echo "$1 rc=$? failed=$(grep -cE '^\[  FAILED  \] [A-Za-z]' $S/logs/h-$1.log)"
}
run hs hven_sqp_tests 'HsBattery.CrashBasisIsANullResultOnTheBatteryWithTwoBoundSeededExceptions:HsBattery.RefinementStepCountersOnBorderRepro'
run scale hven_sqp_tests 'ScaleF7.WideWindowWalkIsChurnNotDegeneracy:F7ColdScaleSmoke.EmptyWindowColdSolveAtTenThousandVariables'
run pred hven_sqp_tests 'PredictorRatioTest.InheritsTheTrueActivitySetAcrossAnActivationThreshold'
run ws hven_sqp_tests 'SqpDriverWarmStart.WarmSeedingKeepsLateSubproblemsCheap:WarmStartBattery.Corpus:WarmStartBattery.CrossoverChainOnTheBridgeFamilyBeatsCold:WarmStart.BudgetModeOffMatchesMaxIterExactly'
run ipqp hven_sqp_tests 'IpqpAcceptanceCensus.AnArmedRunIsNeverMistakenForAStallAcrossTheHsBattery:IpqpDispatch.ASuccessfulSubproblemResetsTheConsecutiveEscapeTally'
run corp hven_sqp_tests 'CorpusTask6bPhaseB.TheShippedKSsnConfigurationIsUnmovedByTheFourLevers'
run modesites hven_sqp_tests 'QpModeSites.*'
run t7a hven_sqp_tests 'QpEngineStructuralViolation.T7ACertifiedInfeasibleExitRETURNSTheRefinedPointInBothAlgebras'
run ac hven_qp_walk_seam_tests 'QpAntiCyclingArm.*'
run deg hven_qp_walk_seam_tests 'QpDegeneracyArm.*'
run ff hven_qp_walk_seam_tests 'QpNc2FlatFace.*'
