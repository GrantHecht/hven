#!/usr/bin/env bash
# perf record attribution leg: one process per (arm,cell), pinned, instructions:u.
R=/home/ghecht/Projects/hven/.claude/worktrees/pf2
DD=$R/docs/notes/data/2026-11-m8-pf12
BIN_u0=$R/.scratch/build-m6-19f7674f/bench/hven_sqp_corpus
BIN_m8=$R/.scratch/build-m8-331b31837/bench/hven_sqp_corpus
read -r a b c d <<< "$BOX_LANE_PHYS"
go() { # core arm cell
  local bin; [ $2 = u0 ] && bin=$BIN_u0 || bin=$BIN_m8
  env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 HVEN_PF12_QP_FAILURE_BUDGET=1 taskset -c $1 \
   perf record -q -e instructions:u -c 100000000 -o $R/.scratch/rec-$2-$3.data -- \
   $bin --internal-run-one $3 --engine walk --internal-out /dev/null > /dev/null 2>&1
  perf script -i $R/.scratch/rec-$2-$3.data -F ip,sym,dso 2>/dev/null > $R/.scratch/rec-$2-$3.script
  echo "done $2 $3"
}
go $a u0 f7_n1000_path_neutral & go $b m8 f7_n1000_path_neutral &
go $c u0 f7_n825_path_neutral_control & go $d m8 f7_n825_path_neutral_control &
wait
