#!/usr/bin/env bash
# FIX2 continuation: the final battery at the comment-fix head (libhven.a byte-identical).
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
$S/leg.sh f3-rel bash -c "cd $W/.scratch/nc2-build-release && MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 setsid ctest -j4 > $S/logs2/ctest-release-final2.log 2>&1; echo rel-rc=\$?"
$S/leg.sh f3-dbg bash -c "cd $W/.scratch/nc2-build-debug && MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 setsid ctest -j4 -E '^Continuation\.ProbeBudgetBoundsAFailingProposal\$' > $S/logs2/ctest-debug-final2.log 2>&1; echo dbg-rc=\$?"
$S/leg.sh f3-long bash -c "cd $W/.scratch/nc2-build-debug && MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 setsid ctest -R '^Continuation\.ProbeBudgetBoundsAFailingProposal\$' > $S/logs2/ctest-debug-final2-long.log 2>&1; echo long-rc=\$?"
echo FINAL3_DONE
