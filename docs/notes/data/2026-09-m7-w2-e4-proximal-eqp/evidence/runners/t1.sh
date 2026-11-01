#!/usr/bin/env bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
B=$W/.scratch/nc2-build-debug
run() { # tag binary filter
  local tag=$1 bin=$2 flt=$3
  MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 $(find $B/tests -name $bin -type f | head -1) --gtest_filter="$flt" \
    > $S/logs/t1-$tag.log 2>&1
  echo "$tag rc=$?  $(grep -cE '^\[       OK \]' $S/logs/t1-$tag.log) ok, $(grep -cE '^\[  FAILED  \] [A-Za-z]' $S/logs/t1-$tag.log) failed-lines"
  grep -E '^\[  FAILED  \] [A-Za-z]' $S/logs/t1-$tag.log | sort -u | head -30
}
run runaway hven_sqp_tests 'QpEngine.UnboundedProblemIsReportedAsNumericalErrorNotOptimal:QpEngine.RunawayWithActiveRowClearsItsMultipliers:QpEngine.EffectivelyInfiniteBoundsStillDetectTheRunaway'
run eqpsolve hven_sqp_tests 'EqpSolve.*:EqpRefinementAb.*'
run census hven_sqp_tests 'QpEngineStructuralViolation.T7*'
run hs hven_sqp_tests 'HsSweepCorpus.*:HsBattery.*'
run border hven_sqp_tests 'QpEngineBorder.*'
run flatface hven_qp_walk_seam_tests 'QpNc2FlatFace.*'
