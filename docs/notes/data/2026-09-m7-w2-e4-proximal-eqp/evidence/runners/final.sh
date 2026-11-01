#!/usr/bin/env bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
TAG=${TAG:-h1}
for cfg in debug release; do
  cmake --build $W/.scratch/nc2-build-$cfg -j 12 > $S/logs/build-$TAG-$cfg.log 2>&1 || { tail -30 $S/logs/build-$TAG-$cfg.log; exit 9; }
  echo "BUILD_OK $cfg"
done
cd $W/.scratch/nc2-build-release || exit 8
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 setsid ctest -j4 > $S/logs/ctest-release-$TAG.log 2>&1
echo "ctest release rc=$?"
tail -3 $S/logs/ctest-release-$TAG.log
TAG=$TAG $S/corp3.sh
