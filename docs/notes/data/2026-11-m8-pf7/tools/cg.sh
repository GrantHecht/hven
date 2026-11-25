#!/usr/bin/env bash
# usage: cg.sh <bin> <lane> <problem> <outprefix>   callgrind of one cell (cwd = bench clone)
set -u
BIN=$1; LANE=$2; P=$3; O=$4
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 TMPDIR=/home/ghecht/Projects/hven/.claude/worktrees/pf7/.scratch/tmp
cd /home/ghecht/Projects/hven-bench-pf7
valgrind --tool=callgrind --callgrind-out-file=$O.cg --dump-instr=no --compress-strings=no --compress-pos=no $BIN --lane $LANE --problem $P --options default --budget-iters 0 --assert-reference > $O.out 2> $O.err
