#!/usr/bin/env bash
# M7 W0 T2 -- THE RESIDUAL'S ATTRIBUTION LEG.
#
# `a-scalars` did not land FLAT, so what is left has to be named. The question
# this leg answers is the one that separates the candidate classes: DOES THE
# RESIDUAL SCALE WITH THE PROBLEM?  An O(1) per-event cost -- the null guard,
# the two mask reads, the clock, the event aggregate, the std::function call --
# would give the SAME per-event delta on a small cell and a large one. An O(n)
# per-event cost would not.
#
# Two cell subsets of the leg, the smallest and the largest, three rounds,
# THREE arms (a00, a-all, a-scalars) alternated within a round -- a-all is here
# too so the reading can separate "what the mask removed" from "what is left"
# on each population rather than only on the whole leg.
# $1 = small|large   $2 = rounds
set -o pipefail
source /home/ghecht/Projects/hven/.scratch/m7t2/env.sh
source $F/common.sh
SIZE=${1:?small|large}
ROUNDS=${2:-3}
case $SIZE in
  small) CELLS="f7_n1000_bound_neutral,f7_n1000_bound_physics,f7_n1000_bound_corrupted,f7_n1000_bound_activity,f7_n1000_bound_warm" ;;
  large) CELLS="f7_n20000_bound_neutral,f7_n20000_bound_physics,f7_n20000_bound_corrupted,f7_n20000_bound_activity,f7_n20000_bound_warm" ;;
  *) echo "unknown size $SIZE"; exit 2 ;;
esac
BATCH="attrib-$SIZE"
OUT=$F/art/raw/attrib/$SIZE
POUT=$F/art/perf/attrib/$SIZE
mkdir -p $OUT $POUT
echo "BATCH $BATCH size=$SIZE rounds=$ROUNDS"
echo "cells: $CELLS"
echo "pin: taskset -c $CORE (SMT sibling $SIB left idle), MKL_NUM_THREADS=OMP_NUM_THREADS=1"
echo "WARMUP $(utc) -- one untimed, unrecorded run"
env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c $CORE \
    $BIN --engine interior --cells "$CELLS" --csv $OUT/warmup.csv > $OUT/warmup.stdout 2>&1
echo "WARMUP_RC=$? (NOT folded: a warm-up is not a measurement)"
batch_start $BATCH
for r in $(seq 1 $ROUNDS); do
  for arm in a00 a-all a-scalars; do
    case $arm in
      a00)       ARMENV=() ;;
      a-all)     ARMENV=(HVEN_LEG_COUNT_CALLBACK=1) ;;
      a-scalars) ARMENV=(HVEN_LEG_COUNT_CALLBACK=1 HVEN_LEG_CALLBACK_PAYLOAD=scalars) ;;
    esac
    echo "RUN  $(utc) size=$SIZE arm=$arm round=$r"
    timed_run "$BATCH/$arm-r$r" "$OUT/$arm-r$r.stdout" -- \
        env "${ARMENV[@]}" MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c $CORE \
        perf stat -e $PA -o $POUT/$arm-r$r.txt -- \
        $BIN --engine interior --cells "$CELLS" --csv $OUT/$arm-r$r.csv
    echo "DONE $(utc) size=$SIZE arm=$arm round=$r rc_so_far=$LEG_RC"
  done
  box_guard "$BATCH/r$r"
done
batch_end $BATCH
echo "LEG_WORST_RC=$LEG_RC"
exit $LEG_RC
