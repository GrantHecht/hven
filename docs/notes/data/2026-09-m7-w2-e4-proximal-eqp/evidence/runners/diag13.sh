#!/usr/bin/env bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
B=$W/.scratch/nc2-build-debug
cmake --build $B -j 12 --target hven_sqp_tests > $S/logs/diag13-build.log 2>&1 || { tail -20 $S/logs/diag13-build.log; exit 9; }
BIN=$(find $B/tests -name hven_sqp_tests -type f | head -1)
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 $BIN --gtest_filter='HsSweepCorpus.Corpus' > $S/logs/diag13.log 2>&1
echo "rc=$?"
grep -E 'E4PROBE|HS[0-9]+ at p =' $S/logs/diag13.log | sed 's/^ *//' | head -30
