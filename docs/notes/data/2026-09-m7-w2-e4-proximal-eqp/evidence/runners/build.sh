#!/usr/bin/env bash
# usage: build.sh <debug|release> <target|all> <logtag>
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
cfg=$1; tgt=$2; tag=$3
if [ "$tgt" = all ]; then
  cmake --build $W/.scratch/nc2-build-$cfg -j 12 > $S/logs/build-$tag.log 2>&1
else
  cmake --build $W/.scratch/nc2-build-$cfg --target $tgt -j 12 > $S/logs/build-$tag.log 2>&1
fi
rc=$?
echo "build $cfg $tgt rc=$rc"
[ $rc -eq 0 ] || tail -60 $S/logs/build-$tag.log
exit $rc
