#!/usr/bin/env bash
# call-graph attribution (dwarf), m8 only, two cells, pinned on two lane CPUs
R=/home/ghecht/Projects/hven/.claude/worktrees/pf2
read -r a b _ <<< "$BOX_LANE_PHYS"
go() { env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 HVEN_PF12_QP_FAILURE_BUDGET=1 taskset -c $1 \
  perf record -q -e instructions:u -c 200000000 --call-graph dwarf,16384 -o $R/.scratch/cg-m8-$2.data -- \
  $R/.scratch/build-m8-331b31837/bench/hven_sqp_corpus --internal-run-one $2 --engine walk --internal-out /dev/null >/dev/null 2>$R/.scratch/cg-$2.err
  echo "done $2 rc=$?"; }
go $a f7_n1000_path_neutral & go $b f7_n825_path_neutral_control & wait
