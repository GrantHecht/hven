#!/usr/bin/env bash
set -o pipefail
S=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9/.scratch/e4-fix5
cmake --build $S/build-rel --parallel 16 > $S/logs/build-hven3.log 2>&1 || { tail -30 $S/logs/build-hven3.log; exit 82; }
cmake --install $S/build-rel > $S/logs/install-hven3.log 2>&1 || exit 83
cmake --build $S/replay-build-diag > $S/logs/build-replay-diag2.log 2>&1 || exit 2
cmake --build $S/bench-build --target hven_bench_cell > $S/logs/build-bench2.log 2>&1 || exit 85
R=$S/replay-build-diag/dump_replay
for d in qp5 qp6 qp7 qp8; do
  for u in "" "1"; do
    printf 'UNGATE=%s %s: ' "${u:-0}" "$d"
    env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 ${u:+HVEN_F5_UNGATE=1} taskset -c 2 \
      $R --e2 1 --algebra border $S/dumps/ACOPR118_e4/$d.txt 2>/dev/null \
      | sed "s|.*/$d.txt|$d|" | cut -c1-150
  done
done
