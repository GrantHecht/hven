#!/usr/bin/env bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
D=$W/docs/notes/data/2026-09-m7-w2-r2-composite-landing/dumps
for c in Cs_STEENBRB/qp1 Cs_STEENBRB/qp3 A0_YORKNET/qp3 A0_YORKNET/qp2; do
  for alg in border refactorize; do
  MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 $S/tools-build-replay/dump_replay --algebra $alg --e2 1 $D/$c.txt > $S/logs/p3.out 2>&1
  echo "$c $alg rc=$? $(tr ' ' '\n' < $S/logs/p3.out | grep -E '^(status|minors|refuse|residiters|f|stat|eq)=' | paste -sd' ' -)"
  done
done
