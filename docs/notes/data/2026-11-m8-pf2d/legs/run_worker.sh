#!/usr/bin/env bash
# PF-2d instruction leg worker (RESULTS-PF-12's run_worker.sh with three arms). $1 = logical cpu (a distinct physical
# core of the lane), rest = cells. Per cell, 3 rounds; arms ALTERNATED within a round (rotating order); one process per
# cell (--internal-run-one). Untimed warm-up first. Arms: u0 = M6 19f7674f (build-u0), new = the pf2d code + the PF-12
# lever patch (build-measB), off = the same binary with independent_working_set = 0 (U0-equivalent work; HVEN_PF12_IWS=0).
# The new and off arms also write the solve-tally sidecar (<csv>.tally) -- HVEN_BENCH_SOLVE_TALLY=1.
R=/home/ghecht/Projects/hven/.claude/worktrees/pf2d
DD=$R/docs/notes/data/2026-11-m8-pf2d/measure
PA=instructions:u,branches:u,cycles:u,branch-misses:u,L1-icache-load-misses:u
CORE=$1; shift
P2B=/home/ghecht/Projects/hven/.claude/worktrees/pf2b
BIN_u0=$P2B/.scratch/build-u0/bench/hven_sqp_corpus
BIN_prev=$P2B/.scratch/build-measB/bench/hven_sqp_corpus
BIN_new=$R/.scratch/build-measD/bench/hven_sqp_corpus
BIN_newG=$R/.scratch/build-measG/bench/hven_sqp_corpus
run() { # arm round cell
  local arm=$1 r=$2 c=$3 bin extra=()
  case $arm in u0) bin=$BIN_u0;; prev) bin=$BIN_prev; extra=(HVEN_BENCH_SOLVE_TALLY=1);; new) bin=$BIN_new; extra=(HVEN_BENCH_SOLVE_TALLY=1);; newG) bin=$BIN_newG; extra=(HVEN_BENCH_SOLVE_TALLY=1);; off) bin=$BIN_new; extra=(HVEN_BENCH_SOLVE_TALLY=1 HVEN_PF12_IWS=0);; esac
  local tag=$arm-r$r-$c
  [ -s $DD/raw/$tag.csv ] && { echo "SKIP $tag"; return; }
  echo "RUN $(date -u +%T) core=$CORE $tag"
  env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 HVEN_PF12_QP_FAILURE_BUDGET=1 "${extra[@]}" taskset -c $CORE \
    perf stat -e $PA -o $DD/perf/$tag.txt -- \
    $bin --internal-run-one $c --engine walk --internal-out $DD/raw/$tag.csv \
    > $DD/raw/$tag.stdout 2> $DD/raw/$tag.stderr
  echo "DONE $(date -u +%T) $tag rc=$?"
}
for bin in $BIN_u0 $BIN_prev $BIN_new $BIN_newG; do
  env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c $CORE $bin --internal-run-one f7_n800_path_warm --engine walk --internal-out /dev/null >/dev/null 2>&1
done
ORD=("u0 prev new off newG" "prev new off newG u0" "newG u0 prev new off")
for c in "$@"; do
  for r in 1 2 3; do
    for a in ${ORD[$((r-1))]}; do run $a $r $c; done
  done
done
