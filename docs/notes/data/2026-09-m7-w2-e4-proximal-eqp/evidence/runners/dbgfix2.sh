#!/usr/bin/env bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
$S/leg.sh dbg-build $S/build.sh debug all j-debug | grep -E "rc=" || exit 1
$S/leg.sh dbg-suite bash -c "cd $W/.scratch/nc2-build-debug && MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 setsid ctest -j4 -E '^Continuation\.ProbeBudgetBoundsAFailingProposal\$' > $S/logs2/ctest-debug-j.log 2>&1; echo ctest-debug-rc=\$?"
$S/leg.sh dbg-long bash -c "cd $W/.scratch/nc2-build-debug && MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 setsid ctest -R '^Continuation\.ProbeBudgetBoundsAFailingProposal\$' > $S/logs2/ctest-debug-j-long.log 2>&1; echo ctest-debug-long-rc=\$?"
echo DEBUG_DONE
