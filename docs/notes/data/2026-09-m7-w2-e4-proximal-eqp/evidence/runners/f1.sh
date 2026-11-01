#!/usr/bin/env bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
cfg=${CFG:-debug}
B=$W/.scratch/nc2-build-$cfg
run() {
  local tag=$1 bin=$2 flt=$3
  MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 $(find $B/tests -name $bin -type f | head -1) --gtest_filter="$flt" \
    > $S/logs/f1-$cfg-$tag.log 2>&1
  echo "$tag rc=$?  ok=$(grep -cE '^\[       OK \]' $S/logs/f1-$cfg-$tag.log) failed=$(grep -cE '^\[  FAILED  \] [A-Za-z]' $S/logs/f1-$cfg-$tag.log)"
  grep -E '^\[  FAILED  \] [A-Za-z]' $S/logs/f1-$cfg-$tag.log | sed 's/ (.*//' | sort -u
}
run b1 hven_sqp_tests 'B1Gate.*'
run hs hven_sqp_tests 'HsSweepCorpus.*:HsBattery.*'
run facerefine hven_sqp_tests 'QpEngineFaceRefine.*'
run runaway hven_sqp_tests 'QpEngine.UnboundedProblemIsReportedAsNumericalErrorNotOptimal:QpEngine.RunawayWithActiveRowClearsItsMultipliers:QpEngine.EffectivelyInfiniteBoundsStillDetectTheRunaway'
run census hven_sqp_tests 'QpEngineStructuralViolation.T7*'
run eqpsolve hven_sqp_tests 'EqpSolve.*:EqpRefinementAb.*'
run indef hven_sqp_tests 'QpEngineIndefinite.*'
run border hven_sqp_tests 'QpEngineBorder.*'
