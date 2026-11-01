#!/usr/bin/env bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
B=$W/.scratch/nc2-build-debug
cmake --build $B -j 12 --target hven_sqp_tests > $S/logs/diag6-build.log 2>&1 || { tail -30 $S/logs/diag6-build.log; exit 9; }
BIN=$(find $B/tests -name hven_sqp_tests -type f | head -1)
E4_DIAG=1 MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 $BIN \
  --gtest_filter='HsBattery.RefactorizeMode:HsBattery.BorderMode' > $S/logs/diag6.log 2>&1
echo "rc=$?"
grep E4DIAG $S/logs/diag6.log | sort | uniq -c | sort -rn | head -25
