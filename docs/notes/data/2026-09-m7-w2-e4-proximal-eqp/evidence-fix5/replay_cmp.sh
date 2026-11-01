#!/usr/bin/env bash
set -o pipefail
S=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9/.scratch/e4-fix5
cd $S
for tag in ff02789b46a7 85dc01ac9f53; do
  echo "===== PIN $tag ====="
  env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
    $S/replay-build-$tag/dump_replay $S/dumps/ACOPR118_e4/qp0.txt $S/dumps/ACOPR118_e4/qp1.txt \
    $S/dumps/ACOPR118_e4/qp2.txt $S/dumps/ACOPR118_e4/qp3.txt $S/dumps/ACOPR118_e4/qp4.txt \
    $S/dumps/ACOPR118_e4/qp5.txt $S/dumps/ACOPR118_e4/qp6.txt $S/dumps/ACOPR118_e4/qp7.txt \
    $S/dumps/ACOPR118_e4/qp8.txt
done
