#!/usr/bin/env bash
set -o pipefail
S=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9/.scratch/e4
D=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9/docs/notes/data/2026-09-m7-w2-e2-d4-feasible-qps/dumps/DALLASM_def
for alg in border refactorize; do
  MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 $S/tools-build-replay/dump_replay --algebra $alg --e2 1 --cert $S/cert $D/qp1.txt $D/qp2.txt
done
