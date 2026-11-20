#!/usr/bin/env bash
# PF-1 contrast: shipped budget (3, env unset), one pinned process.
R=/home/ghecht/Projects/hven/.claude/worktrees/pf2; DD=$R/docs/notes/data/2026-11-m8-pf12
read -r a _ <<< "$BOX_LANE_PHYS"
env -u HVEN_PF12_QP_FAILURE_BUDGET MKL_NUM_THREADS=1 taskset -c $a perf stat -e instructions:u,branches:u,cycles:u -o $DD/perf/m8-budget3-f7_n1000_path_corrupted.txt -- \
 $R/.scratch/build-m8-331b31837/bench/hven_sqp_corpus --internal-run-one f7_n1000_path_corrupted --engine walk --internal-out $DD/raw/m8-budget3-f7_n1000_path_corrupted.csv > /dev/null 2>&1
echo CONTRAST_DONE
