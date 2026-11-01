#!/usr/bin/env bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
cd $W/.scratch/nc2-build-debug || exit 8
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 setsid ctest -j4 -E '^Continuation\.ProbeBudgetBoundsAFailingProposal$' > $S/logs/ctest-debug-h1.log 2>&1
echo "DBG_MAIN_DONE rc=$?"
tail -3 $S/logs/ctest-debug-h1.log
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 setsid ctest -j1 -R '^Continuation\.ProbeBudgetBoundsAFailingProposal$' > $S/logs/ctest-debug-h1-long.log 2>&1
echo "DBG_LONG_DONE rc=$?"
tail -3 $S/logs/ctest-debug-h1-long.log
