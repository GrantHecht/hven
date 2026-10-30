#!/usr/bin/env bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
TAG=${TAG:-g5}
for cfg in debug release; do
  cmake --build $W/.scratch/nc2-build-$cfg -j 12 > $S/logs/build-$TAG-$cfg.log 2>&1 || { tail -30 $S/logs/build-$TAG-$cfg.log; exit 9; }
  echo "BUILD_OK $cfg"
done
$S/g4.sh 1e-5 > $S/logs/$TAG-u0.log 2>&1; echo "u0 rc=$?"
grep -E "differences" $S/logs/$TAG-u0.log
B=$(find $W/.scratch/nc2-build-release/tests -name hven_sqp_tests -type f | head -1)
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 $B --gtest_filter='QpEngineStructuralViolation.T7TheFeasibleCensusHalfOverTheFULLDualMuSet:HsBattery.*:HsSweepCorpus.*:B1Gate.*' > $S/logs/$TAG-key.log 2>&1
echo "key rc=$? failed=$(grep -cE '^\[  FAILED  \] [A-Za-z]' $S/logs/$TAG-key.log)"
grep -E '^\[  FAILED  \] [A-Za-z]' $S/logs/$TAG-key.log | sed 's/ (.*//' | sort -u
cd $W/.scratch/nc2-build-release || exit 8
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 setsid ctest -j4 > $S/logs/ctest-release-$TAG.log 2>&1
echo "ctest release rc=$?"
