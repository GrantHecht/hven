#!/usr/bin/env bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
cmake --build $W/.scratch/nc2-build-release -j 12 > $S/logs/rel-build3.log 2>&1 || { tail -30 $S/logs/rel-build3.log; exit 9; }
echo BUILD_OK
cd $W/.scratch/nc2-build-release || exit 8
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 setsid ctest -j4 > $S/logs/ctest-rel-e4.log 2>&1
echo "ctest rc=$?"
tail -4 $S/logs/ctest-rel-e4.log
grep -E '^\s+[0-9]+ - .*Failed' $S/logs/ctest-rel-e4.log | sed 's/^ *//' | head -60
