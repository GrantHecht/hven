#!/usr/bin/env bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
echo "=== build release all"
cmake --build $W/.scratch/nc2-build-release -j 12 > $S/logs/rel-build.log 2>&1 || { tail -40 $S/logs/rel-build.log; exit 9; }
echo BUILD_OK
echo "=== install + replay tool"
$S/build_tools.sh || exit 8
echo "=== eqpsolve + runaway + border, Release"
BIN=$(find $W/.scratch/nc2-build-release/tests -name hven_sqp_tests -type f | head -1)
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 $BIN \
  --gtest_filter='EqpSolve.*:QpEngine.UnboundedProblemIsReportedAsNumericalErrorNotOptimal:QpEngine.RunawayWithActiveRowClearsItsMultipliers:QpEngine.EffectivelyInfiniteBoundsStillDetectTheRunaway:QpEngineBorder.*' > $S/logs/rel-quick.log 2>&1
echo "quick rc=$? ok=$(grep -cE '^\[       OK \]' $S/logs/rel-quick.log)"
grep -E '^\[  FAILED  \] [A-Za-z]' $S/logs/rel-quick.log | sed 's/ (.*//' | sort -u
