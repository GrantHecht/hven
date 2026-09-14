#!/usr/bin/env bash
# W6 T6 -- THE SQP CORPUS INSTRUCTION LEG.
#
# Each cell is ONE PROCESS (--internal-run-one), which is T8.9r's leg-1 perf
# shape: the public leg forks and execs a child per cell, so a perf stat over
# the leg would count 27 children, a poll loop and 27 execs beside the solve.
# THE FOUR ARMS ARE ALTERNATED AT THE CELL, back to back, so an attached and an
# unattached reading of the same cell are adjacent in time.
#   a00 = --callback off   --trace off    (the shipped shape: nothing attached)
#   a10 = --callback count --trace off
#   a01 = --callback off   --trace sink
#   a11 = --callback count --trace sink
# $1 = walk|ssn|ipm   $2 = round 1|2|3   $3 = cell group (1..N) or 'all'
set -o pipefail
source /home/ghecht/Projects/hven/.scratch/w6t6/env.sh
source $F/common.sh
MODE=${1:?walk|ssn|ipm}
ROUND=${2:?1|2|3}
GROUP=${3:-all}
BATCH="sqpperf-$MODE-r$ROUND-g$GROUP"
OUT=$F/art/raw/sqpperf/$MODE/r$ROUND
POUT=$F/art/perf/sqpperf/$MODE/r$ROUND
mkdir -p $OUT $POUT
ALLCELLS="$(sed -n '3p' $R/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt | tr ',' ' ')"
# The group split exists ONLY to keep a batch inside this session's per-command
# cap; every group is a COMPLETE four-arm alternation for every cell it holds.
case $GROUP in
  all)   CELLS="$ALLCELLS" ;;
  1)     CELLS="$(echo $ALLCELLS | tr ' ' '\n' | sed -n '1,9p'  | tr '\n' ' ')" ;;
  2)     CELLS="$(echo $ALLCELLS | tr ' ' '\n' | sed -n '10,18p'| tr '\n' ' ')" ;;
  3)     CELLS="$(echo $ALLCELLS | tr ' ' '\n' | sed -n '19,27p'| tr '\n' ' ')" ;;
  # An explicit index RANGE, added after walk round 1 measured 13.5 min for
  # group 3 -- longer than this session's per-command cap, which is the only
  # reason the split exists at all. Round 1 ran 1/2/3; rounds 2 and 3 run
  # 1/2/19-22/23-27. The batch boundary is a BATCHING device and nothing else:
  # every cell's four-arm alternation is complete inside whichever batch holds
  # it, which is what the reading pairs on.
  [0-9]*-[0-9]*)
         lo=${GROUP%-*}; hi=${GROUP#*-}
         CELLS="$(echo $ALLCELLS | tr ' ' '\n' | sed -n "${lo},${hi}p" | tr '\n' ' ')" ;;
  *)     echo "unknown group $GROUP"; exit 2 ;;
esac
echo "BATCH $BATCH mode=$MODE round=$ROUND group=$GROUP"
echo "cells: $CELLS"
echo "events: $PA"
echo "pin: taskset -c $CORE (SMT sibling $SIB left idle), MKL_NUM_THREADS=OMP_NUM_THREADS=1"
batch_start $BATCH
for cell in $CELLS; do
  for arm in a00 a10 a01 a11; do
    case $arm in
      a00) CB=off;   TR=off  ;;
      a10) CB=count; TR=off  ;;
      a01) CB=off;   TR=sink ;;
      a11) CB=count; TR=sink ;;
    esac
    echo "RUN  $(utc) mode=$MODE round=$ROUND cell=$cell arm=$arm callback=$CB trace=$TR"
    timed_run "$BATCH/$cell-$arm" "$OUT/$arm-$cell.stderr" -- \
        env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c $CORE \
        perf stat -e $PA -o $POUT/$arm-$cell.txt -- \
        $BIN --internal-run-one $cell --engine $MODE \
             --internal-out $OUT/$arm-$cell.csv --callback $CB --trace $TR
    echo "DONE $(utc) arm=$arm cell=$cell rc_so_far=$LEG_RC"
  done
  box_guard "$BATCH/$cell"
done
batch_end $BATCH
echo "LEG_WORST_RC=$LEG_RC"
exit $LEG_RC
