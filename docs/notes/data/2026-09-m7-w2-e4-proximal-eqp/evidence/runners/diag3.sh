#!/usr/bin/env bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
B=$W/.scratch/nc2-build-debug
BIN=$(find $B/tests -name hven_sqp_tests -type f | head -1)
for m in 2 6; do
E4_OFF=$m E4_DIAG=1 MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 $BIN \
  --gtest_filter='HsBattery.RefactorizeMode' > $S/logs/diag3-m$m.log 2>&1
echo "=== E4_OFF=$m rc=$?"
grep -E '^\[  FAILED  \] [A-Za-z]|HS[0-9]+ \(refactorize' $S/logs/diag3-m$m.log | sed 's/ (.*//' | sort -u | head
grep E4DIAG $S/logs/diag3-m$m.log | sort | uniq -c | sort -rn | head -10
done
