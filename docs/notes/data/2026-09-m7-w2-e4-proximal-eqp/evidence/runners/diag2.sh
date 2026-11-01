#!/usr/bin/env bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
B=$W/.scratch/nc2-build-debug
cmake --build $B -j 12 --target hven_sqp_tests > $S/logs/diag2-build.log 2>&1 || { tail -30 $S/logs/diag2-build.log; exit 9; }
BIN=$(find $B/tests -name hven_sqp_tests -type f | head -1)
E4_DIAG=1 MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 $BIN \
  --gtest_filter='HsBattery.RefactorizeMode' > $S/logs/diag2-hs.log 2>&1
echo "rc=$?"
grep -c E4DIAG $S/logs/diag2-hs.log
grep E4DIAG $S/logs/diag2-hs.log | sort | uniq -c | sort -rn | head -20
