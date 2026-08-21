#!/bin/bash
# Standing bench WALL leg: serial, one solve at a time, alone on the box,
# single-threaded, pinned to one physical core. Alternates base/head so any
# slow machine drift lands on both sides equally.
set -u
BASE=/home/ghecht/Projects/hven/.scratch/task-2b/basebuild/bench/hven_sqp_bench
HEAD=/home/ghecht/Projects/hven/.scratch/task-2b/build/bench/hven_sqp_bench
OUT=/home/ghecht/Projects/hven/.scratch/task-2b/ab/wall
mkdir -p "$OUT"
sum_wall() { awk -F, 'NR>1 {s+=$19} END {printf "%.6f", s}' "$1"; }
run() { # side bin arm-args tag rep
  local side=$1 bin=$2 tag=$4 rep=$5; shift 5
  MKL_NUM_THREADS=1 taskset -c 2 "$bin" $ARGS --csv "$OUT/${tag}_${side}_${rep}.csv" >/dev/null 2>&1
  echo -n "$(sum_wall "$OUT/${tag}_${side}_${rep}.csv") "
}
for spec in "F3n1000cold:--family F3 --n 1000 --arm cold --sweep 5" \
            "F3n1000warm:--family F3 --n 1000 --arm warm --sweep 5" \
            "F7n200cold:--family F7 --n 200 --arm cold --sweep 3"; do
  tag="${spec%%:*}"; ARGS="${spec#*:}"
  echo "=== $tag : $ARGS"
  echo -n "  base: "; for r in 1 2 3 4 5; do run base "$BASE" x "$tag" "$r"; done; echo
  echo -n "  head: "; for r in 1 2 3 4 5; do run head "$HEAD" x "$tag" "$r"; done; echo
done
