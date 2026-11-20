#!/usr/bin/env bash
# peak RSS (informational), A vs B, co-run on the lane's physical cores
S=/home/ghecht/Projects/hven/.claude/worktrees/pf5-proto/.scratch
BIN=${CELL_BIN:-$S/bin/proto/hven_bench_cell}; export TMPDIR=$S/tmp; cd $S/hb; mkdir -p $S/rss
read -ra C <<< "$BOX_LANE_PHYS"; i=0
for P in ACOPP118 YORKNET CRESC100 STEENBRC; do for arm in A B; do
  core=${C[$((i%4))]}; i=$((i+1))
  ( if [ $arm = B ]; then export HVEN_PF5_RETAIN=8; else unset HVEN_PF5_RETAIN; fi
    /usr/bin/time -f "RSS $P $arm maxrss_kb=%M" -o $S/rss/$P.$arm.txt taskset -c $core $BIN --lane hven-sqp --problem $P --options default --budget-iters 0 > /dev/null 2>&1 ) &
  [ $((i%4)) = 0 ] && wait
done; done; wait; cat $S/rss/*.txt
