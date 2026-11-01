#!/usr/bin/env bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
B=$(find $W/.scratch/nc2-build-release/tests -name hven_sqp_tests -type f | head -1)
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 $B --gtest_filter='QpEngineStructuralViolation.T7TheFeasibleCensusHalfOverTheFULLDualMuSet' > $S/logs/g4-census.log 2>&1
echo "census rc=$? ok=$(grep -cE '^\[       OK \]' $S/logs/g4-census.log)"
grep -E "120|census" $S/logs/g4-census.log | tail -5
$S/g4.sh 1e-5 > $S/logs/g4-u0.log 2>&1
echo "u0 rc=$?"
grep -E "differences|CMP_RC|ran head" $S/logs/g4-u0.log | tail -12
