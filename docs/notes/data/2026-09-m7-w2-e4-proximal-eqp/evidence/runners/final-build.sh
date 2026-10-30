#!/usr/bin/env bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
for cfg in debug release; do
  cmake --build $W/.scratch/nc2-build-$cfg -j 12 > $S/logs/final-build-$cfg.log 2>&1
  rc=$?
  echo "final build $cfg rc=$rc"
  [ $rc -eq 0 ] || tail -25 $S/logs/final-build-$cfg.log
done
BIN=$(find $W/.scratch/nc2-build-release/tests -name hven_sqp_tests -type f | head -1)
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 $BIN \
  --gtest_filter='EqpSolve.*:QpEngine.UnboundedProblemIsReportedAsNumericalErrorNotOptimal:QpEngine.RunawayWithActiveRowClearsItsMultipliers:QpEngine.EffectivelyInfiniteBoundsStillDetectTheRunaway:QpEngineStructuralViolation.T7TheFeasibleCensusHalfOverTheFULLDualMuSet' > $S/logs/final-quick.log 2>&1
echo "quick rc=$? ok=$(grep -cE '^\[       OK \]' $S/logs/final-quick.log)"
grep -E '^\[  FAILED  \] [A-Za-z]' $S/logs/final-quick.log | sed 's/ (.*//' | sort -u
