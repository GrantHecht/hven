#!/usr/bin/env bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
$S/leg.sh d13-build $S/build.sh debug all j2-debug | grep -E "rc=" || exit 1
$S/leg.sh d13-rel $S/build.sh release all j2-release | grep -E "rc=" || exit 1
F='CorpusTask6bPhaseB.TheShippedKSsnConfigurationIsUnmovedByTheFourLevers:HsSweepCorpus.Corpus:IpqpAcceptanceCensus.AnArmedRunIsNeverMistakenForAStallAcrossTheHsBattery:SqpDriverCertifiedFallback.T3TheTierSolvedRungReachesTheSameVerdictOnTheSameLinearization:SqpDriverCertifiedFallback.T7F1TheSameInconsistentLinearizationInAllThreeModes:SqpDriverQpFailure.RepeatedQpMaxIterStillPropagates:SqpDriverRestorationExcursion.*:SqpDriverRestorationProximity.TheProxyWouldCertifyWhereTheExactResidualRefuses:SqpDriverRestorationSeed.TheSeedsEvaluationIsCountedExactlyOnce:WarmStartBattery.Corpus'
$S/leg.sh d13-sqp bash -c "B=\$(find $W/.scratch/nc2-build-debug/tests -name hven_sqp_tests -type f | head -1); MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \$B --gtest_filter='$F' > $S/logs2/d13-sqp.log 2>&1; echo sqp-rc=\$?"
$S/leg.sh d13-walk bash -c "B=\$(find $W/.scratch/nc2-build-debug/tests -name hven_qp_walk_seam_tests -type f | head -1); MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \$B --gtest_filter='QpNc2FlatFace.TheSteenbrbRestorationLpQp4*:QpE4ProximalEqp.*' > $S/logs2/d13-walk.log 2>&1; echo walk-rc=\$?"
echo D13_DONE
