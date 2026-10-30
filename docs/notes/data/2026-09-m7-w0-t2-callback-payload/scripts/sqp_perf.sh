#!/usr/bin/env bash
# M7 W0 T2 -- THE SQP CORPUS INSTRUCTION LEG, WALK MODE ONLY, THREE ARMS.
#
# Each cell is ONE PROCESS (--internal-run-one), which is W6 T6's and T8.9r's
# perf shape: the public leg forks and execs a child per cell, so a perf stat
# over the leg would count the forks, the execs and a parent poll loop beside
# the solve.  THE THREE ARMS ARE ALTERNATED AT THE CELL, back to back.
#   a00       --callback off            the shipped shape
#   a-all     --callback count          T6's arm (kAll)
#   a-scalars --callback count-scalars  the payload mask
# $1 = round   $2 = cell group ('all', 1|2|3, or lo-hi)
set -o pipefail
source /home/ghecht/Projects/hven/.scratch/m7t2/env.sh
source $F/common.sh
ROUND=${1:?round}
GROUP=${2:-all}
MODE=walk
BATCH="sqpperf-$MODE-r$ROUND-g$GROUP"
OUT=$F/art/raw/sqpperf/$MODE/r$ROUND
POUT=$F/art/perf/sqpperf/$MODE/r$ROUND
mkdir -p $OUT $POUT
ALLCELLS="$(sed -n '3p' $R/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt | tr ',' ' ')"
# The group split exists ONLY to keep a batch inside this session's per-command
# cap; every group is a COMPLETE three-arm alternation for every cell it holds.
case $GROUP in
  all) CELLS="$ALLCELLS" ;;
  [0-9]*-[0-9]*)
       lo=${GROUP%-*}; hi=${GROUP#*-}
       CELLS="$(echo $ALLCELLS | tr ' ' '\n' | sed -n "${lo},${hi}p" | tr '\n' ' ')" ;;
  *)   echo "unknown group $GROUP"; exit 2 ;;
esac
echo "BATCH $BATCH mode=$MODE round=$ROUND group=$GROUP"
echo "cells: $CELLS"
echo "events: $PA"
echo "pin: taskset -c $CORE (SMT sibling $SIB left idle), MKL_NUM_THREADS=OMP_NUM_THREADS=1"
batch_start $BATCH
for cell in $CELLS; do
  for arm in a00 a-all a-scalars; do
    case $arm in
      a00)       CB=off ;;
      a-all)     CB=count ;;
      a-scalars) CB=count-scalars ;;
    esac
    echo "RUN  $(utc) mode=$MODE round=$ROUND cell=$cell arm=$arm callback=$CB"
    timed_run "$BATCH/$cell-$arm" "$OUT/$arm-$cell.stderr" -- \
        env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c $CORE \
        perf stat -e $PA -o $POUT/$arm-$cell.txt -- \
        $BIN --internal-run-one $cell --engine $MODE \
             --internal-out $OUT/$arm-$cell.csv --callback $CB --trace off
    echo "DONE $(utc) arm=$arm cell=$cell rc_so_far=$LEG_RC"
  done
  box_guard "$BATCH/$cell"
done
batch_end $BATCH
echo "LEG_WORST_RC=$LEG_RC"
exit $LEG_RC
