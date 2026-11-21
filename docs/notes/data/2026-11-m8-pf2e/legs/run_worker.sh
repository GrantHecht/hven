#!/usr/bin/env bash
# PF-2e P3 worker (pf2d's run_worker.sh with two arms). $1 = logical cpu (a distinct physical core of the lane), rest = cells.
# Per cell, 3 rounds; arms alternated within a round (rotating order); one process per cell (--internal-run-one); untimed
# warm-up first. Arms: newL = pf2d's measL binary (control re-take), newE = git archive E + the PF-12 lever patch (build-measE).
# Both write the solve-tally sidecar (<csv>.tally, HVEN_BENCH_SOLVE_TALLY=1).
R=/home/ghecht/Projects/hven/.claude/worktrees/pf2e
DD=$R/docs/notes/data/2026-11-m8-pf2e/measure
PA=instructions:u,branches:u,cycles:u,branch-misses:u,L1-icache-load-misses:u
CORE=$1; shift
BIN_newL=/home/ghecht/Projects/hven/.claude/worktrees/pf2d/.scratch/build-measL/bench/hven_sqp_corpus
BIN_newE=$R/.scratch/build-pf2e/measE/bench/hven_sqp_corpus
run() { # arm round cell
  local arm=$1 r=$2 c=$3 bin
  case $arm in newL) bin=$BIN_newL;; newE) bin=$BIN_newE;; esac
  local tag=$arm-r$r-$c
  [ -s $DD/raw/$tag.csv ] && { echo "SKIP $tag"; return; }
  echo "RUN $(date -u +%T) core=$CORE $tag"
  env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 HVEN_PF12_QP_FAILURE_BUDGET=1 HVEN_BENCH_SOLVE_TALLY=1 taskset -c $CORE \
    perf stat -e $PA -o $DD/perf/$tag.txt -- \
    $bin --internal-run-one $c --engine walk --internal-out $DD/raw/$tag.csv \
    > $DD/raw/$tag.stdout 2> $DD/raw/$tag.stderr
  echo "DONE $(date -u +%T) $tag rc=$?"
}
for bin in $BIN_newL $BIN_newE; do
  env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c $CORE $bin --internal-run-one f7_n800_path_warm --engine walk --internal-out /dev/null >/dev/null 2>&1
done
ORD=("newL newE" "newE newL" "newL newE")
for c in "$@"; do
  for r in 1 2 3; do
    for a in ${ORD[$((r-1))]}; do run $a $r $c; done
  done
done
