#!/usr/bin/env bash
# One corpus leg per flock acquisition, each well under 600 s. usage: corp_fix2.sh <TAG>
set -o pipefail
S=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9/.scratch/e4
TAG=$1
$S/leg.sh ${TAG}-tools $S/build_tools.sh "-DNC2_COUNTERS=1" | grep -E "TOOLS_OK|rc=" || exit 8
for alg in border refactorize; do
  for lst in old36 new15; do
    $S/leg.sh ${TAG}-$alg-$lst $S/corpus.sh $TAG $alg $S/$lst.list 1 | grep -E "^corpus|PGREP_NOT" || echo "FAILED $TAG $alg $lst"
  done
done
for lst in old36 new15; do
  $S/leg.sh ${TAG}off-border-$lst $S/corpus.sh ${TAG}off border $S/$lst.list 0 | grep -E "^corpus|PGREP_NOT" || echo "FAILED ${TAG}off border $lst"
done
echo CORPUS_DONE
