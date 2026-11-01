#!/usr/bin/env bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
$S/build_tools.sh "-DNC2_COUNTERS=1" || exit 8
D=$W/docs/notes/data/2026-09-m7-w2-e2-d4-feasible-qps/dumps
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 $S/tools-build-replay/dump_replay --algebra border --e2 1 \
  $D/Cs_STEENBRB/qp1.txt $D/A0_YORKNET/qp3.txt $D/ACOPP57_def/qp0.txt 2>&1 | \
  sed 's#.*/dumps/##' | tr ' ' '\n' | grep -E 'qp[0-9]|status|minors=|refuse|residiters|f=|stat=' | paste -sd' ' -
