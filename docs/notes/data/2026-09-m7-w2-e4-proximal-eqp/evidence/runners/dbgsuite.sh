#!/usr/bin/env bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
TAG=${TAG:-g5}
cd $W/.scratch/nc2-build-debug || exit 8
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 setsid ctest -j4 -E '^Continuation\.ProbeBudgetBoundsAFailingProposal$' > $S/logs/ctest-debug-$TAG.log 2>&1
echo "ctest debug rc=$?"
tail -3 $S/logs/ctest-debug-$TAG.log
