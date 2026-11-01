#!/usr/bin/env bash
# FIX ROUND 3 battery: builds, the 51-dump corpus (E2-on, both algebras), the key
# tests (T7 census + HsBattery + HsSweepCorpus + B1Gate), U0, both suites, the long
# test alone. Every leg takes the box lock for itself (leg.sh: flock, no timeout,
# markers, in-lock pgrep). Run DETACHED; one solver at a time throughout.
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
mkdir -p $S/logs3

$S/leg.sh k3-build-dbg $S/build.sh debug all k3-debug || exit 1
$S/leg.sh k3-build-rel $S/build.sh release all k3-release || exit 1

$S/leg.sh k3-tools $S/build_tools.sh "-DNC2_COUNTERS=1" || exit 8
for alg in border refactorize; do
  for lst in old36 new15; do
    $S/leg.sh k3-$alg-$lst $S/corpus.sh k3 $alg $S/$lst.list 1 || echo "CORPUS_LEG_FAILED $alg $lst"
  done
done
echo CORPUS_DONE

$S/leg.sh k3-key bash -c "B=\$(find $W/.scratch/nc2-build-release/tests -name hven_sqp_tests -type f | head -1); MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \$B --gtest_filter='QpEngineStructuralViolation.T7TheFeasibleCensusHalfOverTheFULLDualMuSet:HsBattery.*:HsSweepCorpus.*:B1Gate.*' > $S/logs3/k3-key.log 2>&1; echo key-rc=\$?"
$S/leg.sh k3-u0 bash -c "$S/g4.sh 1e-5 > $S/logs3/k3-u0.log 2>&1; echo u0-rc=\$?"

$S/leg.sh k3-rel bash -c "cd $W/.scratch/nc2-build-release && MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 setsid ctest -j4 > $S/logs3/ctest-release-k3.log 2>&1; echo rel-rc=\$?"
$S/leg.sh k3-dbg bash -c "cd $W/.scratch/nc2-build-debug && MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 setsid ctest -j4 -E '^Continuation\.ProbeBudgetBoundsAFailingProposal\$' > $S/logs3/ctest-debug-k3.log 2>&1; echo dbg-rc=\$?"
$S/leg.sh k3-long bash -c "cd $W/.scratch/nc2-build-debug && MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 setsid ctest -R '^Continuation\.ProbeBudgetBoundsAFailingProposal\$' > $S/logs3/ctest-debug-k3-long.log 2>&1; echo long-rc=\$?"
echo FIX3_DONE
