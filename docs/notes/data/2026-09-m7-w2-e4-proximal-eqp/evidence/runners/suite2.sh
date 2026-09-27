#!/usr/bin/env bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
cfg=${CFG:-release}
tag=${TAG:-f1}
cd $W/.scratch/nc2-build-$cfg || exit 8
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 setsid ctest -j4 > $S/logs/ctest-$cfg-$tag.log 2>&1
echo "ctest rc=$?"
tail -4 $S/logs/ctest-$cfg-$tag.log
