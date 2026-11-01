#!/usr/bin/env bash
set -o pipefail
S=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9/.scratch/e4
for alg in border refactorize; do
  MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 $S/tools-build-replay/dump_replay --algebra $alg --e2 1 $(cat $S/drift.list) | sed "s|^.*/dumps/|$alg |" | cut -c1-120
done
