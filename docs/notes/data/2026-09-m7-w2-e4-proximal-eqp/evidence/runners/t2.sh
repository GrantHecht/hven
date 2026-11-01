#!/usr/bin/env bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
B=$W/.scratch/nc2-build-debug
run() {
  local tag=$1 bin=$2 flt=$3
  MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 $(find $B/tests -name $bin -type f | head -1) \
    --gtest_filter="$flt" > $S/logs/$tag.log 2>&1
  echo "$tag rc=$?"
  grep -E '^\[==========\] .* ran|^\[  PASSED  \]|^\[  FAILED  \] [0-9]' $S/logs/$tag.log | tail -3
  grep -E '^\[  FAILED  \] [A-Za-z]' $S/logs/$tag.log | sed 's/ (.*//' | sort -u
}
run t2-border hven_sqp_tests 'QpEngineBorder.*'
run t2-eqpsolve hven_sqp_tests 'EqpSolve.*'
run t2-census hven_sqp_tests 'QpEngineStructuralViolation.T7*'
run t2-hs hven_sqp_tests 'HsSweepCorpus.*:HsBattery.*'
