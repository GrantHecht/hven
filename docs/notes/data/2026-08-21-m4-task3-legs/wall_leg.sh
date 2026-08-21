#!/bin/bash
# Standing bench WALL leg, the carve's protocol: serial (one solve at a time,
# alone on the box), MKL_NUM_THREADS=1, pinned to one physical core,
# base/head ALTERNATED per rep so machine drift lands on both sides.
# wall = the bench CSV's wall_seconds column summed per run; medians of 5 reps.
# Recorded as a neutrality check: this bench drives SqpDriver, which does not
# reach InteriorPointSolver, so it cannot see this task's diff. The IPM's own
# wall leg is ../ab/ipm_wall_leg.sh.
set -u
ROOT=/home/ghecht/Projects/hven/.scratch/task-3
BASE=$ROOT/basebuild/bench/hven_sqp_bench
HEAD=$ROOT/build/bench/hven_sqp_bench
OUT=$ROOT/wall
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
