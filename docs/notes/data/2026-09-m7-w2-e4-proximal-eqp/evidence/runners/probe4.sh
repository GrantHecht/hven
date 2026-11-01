#!/usr/bin/env bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
B=$W/.scratch/nc2-build-debug
cmake --build $B -j 12 --target hven_qp_walk_seam_tests > $S/logs/probe4-build.log 2>&1 || { tail -30 $S/logs/probe4-build.log; exit 9; }
BIN=$(find $B/tests -name hven_qp_walk_seam_tests -type f | head -1)
D=$W/docs/notes/data/2026-09-m7-w2-r2-composite-landing/dumps
for c in Cs_STEENBRB/qp1 A0_YORKNET/qp3; do
  E4_DUMP=$D/$c.txt E4_N=4 MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 $BIN --gtest_filter='E4Scratch.Dump' 2>&1 | grep -E '^===|^  m=' | sed "s#$D/##"
done
