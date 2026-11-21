#!/usr/bin/env bash
# usage: rerun_outliers.sh <out> <lane> <problem>   alone, 7 alternating runs each of A and B (one pinned cpu), instructions:u only.  Under `box-lane --`.
W=/home/ghecht/Projects/hven/.claude/worktrees/pf7; OUT=$1; L=$2; P=$3
cpu=$(echo $BOX_LANE_PHYS | awk '{print $1}')
cd /home/ghecht/Projects/hven-bench-pf7; : > $OUT
for i in 1 2 3 4 5 6 7; do for v in A B2; do
  n=$(taskset -c $cpu perf stat -x, -e instructions:u -- $W/.scratch/bin/cell-$v --lane $L --problem $P --options default --budget-iters 0 --assert-reference 2>&1 >/dev/null | awk -F, '/instructions/{print $1}')
  echo "$v $L $P $n" >> $OUT
done; done
