#!/usr/bin/env bash
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf2b
O=$WT/.scratch/sz
BIN=$WT/.scratch/build-measC/bench/hven_sqp_corpus
read -r a b c d <<< "$BOX_LANE_PHYS"
run() { core=$1; shift; for cell in "$@"; do
  env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 HVEN_PF12_QP_FAILURE_BUDGET=1 HVEN_BENCH_SOLVE_TALLY=1 HVEN_SIZING_LOG=$O/$cell.log \
   taskset -c $core $BIN --internal-run-one $cell --engine walk --internal-out $O/$cell.csv > $O/$cell.stdout 2> $O/$cell.stderr; echo "$cell rc=$?"; done; }
run $a f7_n1000_path_corrupted f7_n800_path_corrupted &
run $b f7_n1000_path_neutral f7_n2000_path_activity &
run $c f7_n800_path_neutral f7_n750_path_neutral_control &
run $d f7_n825_path_neutral_control f7_n2000_path_physics &
wait
