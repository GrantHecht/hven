#!/usr/bin/env bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
TAG=${TAG:-g1}
$S/build_tools.sh "-DNC2_COUNTERS=1" || exit 8
for alg in border refactorize; do
  for lst in old36 new15; do
    $S/corpus.sh $TAG $alg $S/$lst.list 1 || echo "FAILED $TAG $alg $lst"
  done
done
for lst in old36 new15; do
  $S/corpus.sh ${TAG}off border $S/$lst.list 0 || echo "FAILED ${TAG}off border $lst"
done
