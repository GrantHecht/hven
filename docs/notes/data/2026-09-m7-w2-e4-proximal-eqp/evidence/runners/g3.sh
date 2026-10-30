#!/usr/bin/env bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
cmake --build $W/.scratch/nc2-build-release -j 12 > $S/logs/build-g3-rel.log 2>&1 || { tail -30 $S/logs/build-g3-rel.log; exit 9; }
echo BUILD_OK
TAG=g3 $S/corp3.sh
