#!/bin/bash
# Standing bench WALL leg, run for the Level 2 consumption switch.
# Protocol verbatim from docs/notes/data/2026-08-21-m4-task2b-wall/wall_leg.sh
# and docs/notes/data/2026-08-21-m4-task3-legs/wall_leg.sh: serial (one solve at
# a time, alone on the box), MKL_NUM_THREADS=1, pinned to one physical core,
# base/head ALTERNATED per rep so machine drift lands on both sides equally.
# wall = the bench CSV's wall_seconds column summed per run; 5 reps per cell.
# Base arm = 07d5ee1 (the Task 5 grant commit), head arm = branch m4 HEAD.
set -u
ROOT=/home/ghecht/Projects/hven/.scratch/task-5
BASE=$ROOT/basebuild/bench/hven_sqp_bench
HEAD=$ROOT/build/bench/hven_sqp_bench
OUT=$ROOT/wall/runs
mkdir -p "$OUT"
sum_wall() { awk -F, 'NR>1 {s+=$19} END {printf "%.6f", s}' "$1"; }
for spec in "F3n1000cold:--family F3 --n 1000 --arm cold --sweep 5" \
            "F3n1000warm:--family F3 --n 1000 --arm warm --sweep 5" \
            "F7n200cold:--family F7 --n 200 --arm cold --sweep 3"; do
  tag="${spec%%:*}"; ARGS="${spec#*:}"
  echo "=== $tag : $ARGS"
  for r in 1 2 3 4 5; do
    for side in base head; do
      bin=$BASE; [ "$side" = head ] && bin=$HEAD
      MKL_NUM_THREADS=1 taskset -c 2 "$bin" $ARGS --csv "$OUT/${tag}_${side}_${r}.csv" >/dev/null 2>&1
      echo "  rep$r $side $(sum_wall "$OUT/${tag}_${side}_${r}.csv")"
    done
  done
done
