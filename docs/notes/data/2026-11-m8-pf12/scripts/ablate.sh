#!/usr/bin/env bash
# rebuild lever binary (incremental) then run independent_working_set OFF (HVEN_PF12_IWS=0), 2 cells, pinned.
R=/home/ghecht/Projects/hven/.claude/worktrees/pf2; DD=$R/docs/notes/data/2026-11-m8-pf12
CCACHE_DISABLE=1 cmake --build $R/.scratch/build-m8-331b31837 --target hven_sqp_corpus -j${BOX_LANE_JOBS:-4} > $R/.scratch/build-m8-ablate.log 2>&1 || { echo BUILD_FAIL; exit 1; }
sha256sum $R/.scratch/build-m8-331b31837/bench/hven_sqp_corpus
read -r a b _ <<< "$BOX_LANE_PHYS"
go() { env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 HVEN_PF12_QP_FAILURE_BUDGET=1 HVEN_PF12_IWS=0 taskset -c $1 \
 perf stat -e instructions:u,branches:u,cycles:u -o $DD/perf/m8iwsoff-$2.txt -- \
 $R/.scratch/build-m8-331b31837/bench/hven_sqp_corpus --internal-run-one $2 --engine walk --internal-out $DD/raw/m8iwsoff-$2.csv >/dev/null 2>&1; echo "done $2 rc=$?"; }
go $a f7_n1000_path_neutral & go $b f7_n825_path_neutral_control & wait
