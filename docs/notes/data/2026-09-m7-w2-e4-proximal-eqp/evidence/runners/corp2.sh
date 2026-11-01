#!/usr/bin/env bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
cmake --build $W/.scratch/nc2-build-release -j 12 > $S/logs/rel-build2.log 2>&1 || { tail -40 $S/logs/rel-build2.log; exit 9; }
echo BUILD_OK
$S/build_tools.sh "-DNC2_COUNTERS=1" || exit 8
for alg in border refactorize; do
  for lst in old36 new15; do
    $S/corpus.sh e4 $alg $S/$lst.list 1 || echo "FAILED e4 $alg $lst"
  done
done
for lst in old36 new15; do
  $S/corpus.sh e4off border $S/$lst.list 0 || echo "FAILED e4off border $lst"
done
