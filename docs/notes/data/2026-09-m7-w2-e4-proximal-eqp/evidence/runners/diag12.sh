#!/usr/bin/env bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
B=$W/.scratch/nc2-build-debug
BIN=$(find $B/tests -name hven_sqp_tests -type f | head -1)
for m in 0 8 15; do
E4_OFF=$m MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 $BIN \
  --gtest_filter='HsSweepCorpus.*' > $S/logs/diag12-m$m.log 2>&1
echo "=== E4_OFF=$m rc=$? fails=$(grep -c 'Failure' $S/logs/diag12-m$m.log)"
grep -E '^\[  FAILED  \] [A-Za-z]' $S/logs/diag12-m$m.log | sed 's/ (.*//' | sort -u | sed 's/^/   /'
grep -E 'actual: [0-9.]+ vs 1e-06' $S/logs/diag12-m$m.log | head -3 | sed 's/^/   /'
grep -oE 'HS[0-9]+ at p = [0-9.]+' $S/logs/diag12-m$m.log | sort -u | tr '\n' ' ' | sed 's/^/   cells: /'; echo
done
