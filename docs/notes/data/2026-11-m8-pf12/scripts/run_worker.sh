#!/usr/bin/env bash
# PF-12 instruction leg worker. $1 = physical-core cpu id, rest = cells.
# Per cell, 3 rounds; arms (u0 = M6 19f7674f, m8 = 331b31837+lever) ALTERNATED within a round,
# order swapped each round. One process per cell (--internal-run-one). Untimed warm-up first.
R=/home/ghecht/Projects/hven/.claude/worktrees/pf2
DD=$R/docs/notes/data/2026-11-m8-pf12
PA=instructions:u,branches:u,cycles:u,branch-misses:u,L1-icache-load-misses:u
CORE=$1; shift
BIN_u0=$R/.scratch/build-m6-19f7674f/bench/hven_sqp_corpus
BIN_m8=$R/.scratch/build-m8-331b31837/bench/hven_sqp_corpus
run() { # arm round cell
  local arm=$1 r=$2 c=$3 bin
  [ $arm = u0 ] && bin=$BIN_u0 || bin=$BIN_m8
  local tag=$arm-r$r-$c
  [ -s $DD/raw/$tag.csv ] && { echo "SKIP $tag"; return; }
  echo "RUN $(date -u +%T) core=$CORE $tag"
  env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 HVEN_PF12_QP_FAILURE_BUDGET=1 taskset -c $CORE \
    perf stat -e $PA -o $DD/perf/$tag.txt -- \
    $bin --internal-run-one $c --engine walk --internal-out $DD/raw/$tag.csv \
    > $DD/raw/$tag.stdout 2> $DD/raw/$tag.stderr
  echo "DONE $(date -u +%T) $tag rc=$?"
}
# warm-up (untimed, unrecorded)
for bin in $BIN_u0 $BIN_m8; do
  env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c $CORE $bin --internal-run-one f7_n800_path_warm --engine walk --internal-out /dev/null >/dev/null 2>&1
done
for c in "$@"; do
  for r in 1 2 3; do
    if [ $((r % 2)) = 1 ]; then run u0 $r $c; run m8 $r $c; else run m8 $r $c; run u0 $r $c; fi
  done
done
