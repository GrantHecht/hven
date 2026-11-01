#!/usr/bin/env bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
D=$W/docs/notes/data/2026-09-m7-w2-e2-d4-feasible-qps/dumps
for c in Cs_STEENBRB/qp1 A0_YORKNET/qp3 ACOPP57_def/qp0; do
  echo "--- $c"
  MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 $S/tools-build-replay/dump_replay --algebra border --e2 1 $D/$c.txt > $S/logs/probe2-$(basename $c).out 2> $S/logs/probe2-$(basename $c).err
  echo "rc=$?"
  cat $S/logs/probe2-$(basename $c).err | head -5
  cut -c1-40 $S/logs/probe2-$(basename $c).out | sed 's#^.*##'
  tr ' ' '\n' < $S/logs/probe2-$(basename $c).out | grep -E '^(status|minors|refuse|residiters|f|stat)=' | paste -sd' ' -
done
