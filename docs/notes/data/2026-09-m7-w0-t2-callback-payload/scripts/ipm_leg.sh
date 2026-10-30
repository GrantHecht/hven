#!/usr/bin/env bash
# M7 W0 T2 -- THE TOP-LEVEL INTERIOR-POINT LEG (bench/ipm_corpus_leg.cpp, driven
# by --engine interior), FOUR ARMS, W6 T6's protocol on the leg T6 called MOVED:
#   a00       nothing attached                       -- the shipped shape, the denominator
#   a-all     HVEN_LEG_COUNT_CALLBACK=1              -- T6's arm: the counting callback at kAll
#   a-scalars  + HVEN_LEG_CALLBACK_PAYLOAD=scalars   -- THE REMEDY: the same callback at kScalars
#   a-sink    HVEN_LEG_TRACE_SINK=1                  -- the sink alone (T6's declared omission)
# ONE UNTIMED, UNRECORDED WARM-UP runs ahead of the batch, as T6's batch 2 did,
# so no round is the batch's first process (T6's batch 1 round-1 unattached
# process read +2.1 % as a first-process effect).
# $1 = wall|perf   $2 = rounds (wall: 1, perf: 3)   $3 = batch tag (default ipmleg)
#
# THE TAG names the BATCH and its output subtree.  Two perf batches are taken,
# as W6 T6 took two: a reading should not rest on a median that discards an
# outlier, so `ipmleg2` re-takes the whole leg and the artifact states both.
set -o pipefail
source /home/ghecht/Projects/hven/.scratch/m7t2/env.sh
source $F/common.sh
PHASE=${1:?wall|perf}
ROUNDS=${2:-3}
TAG=${3:-ipmleg}
BATCH="$TAG-$PHASE"
OUT=$F/art/raw/$TAG
POUT=$F/art/perf/$TAG
mkdir -p $OUT $POUT
CELLS="$(sed -n '3p' $R/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)"
arm_env() {
  case $1 in
    a00)       ARMENV=() ;;
    a-all)     ARMENV=(HVEN_LEG_COUNT_CALLBACK=1) ;;
    a-scalars) ARMENV=(HVEN_LEG_COUNT_CALLBACK=1 HVEN_LEG_CALLBACK_PAYLOAD=scalars) ;;
    a-sink)    ARMENV=(HVEN_LEG_TRACE_SINK=1) ;;
    *) echo "unknown arm $1"; exit 2 ;;
  esac
}
echo "BATCH $BATCH phase=$PHASE rounds=$ROUNDS"
echo "events: $PA"
echo "pin: taskset -c $CORE (SMT sibling $SIB left idle), MKL_NUM_THREADS=OMP_NUM_THREADS=1"
echo "WARMUP $(utc) -- one untimed, unrecorded run; its counters go nowhere"
env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c $CORE \
    $BIN --engine interior --cells "$CELLS" --csv $OUT/warmup-$PHASE.csv \
    > $OUT/warmup-$PHASE.stdout 2>&1
echo "WARMUP_RC=$? (NOT folded: a warm-up is not a measurement)"
batch_start $BATCH
for r in $(seq 1 $ROUNDS); do
  for arm in a00 a-all a-scalars a-sink; do
    arm_env $arm
    echo "RUN  $(utc) phase=$PHASE arm=$arm round=$r"
    case $PHASE in
      wall)
        timed_run "$BATCH/$arm-r$r" "$OUT/wall-$arm-r$r.stdout" -- \
            env "${ARMENV[@]}" MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c $CORE \
            $BIN --engine interior --cells "$CELLS" --csv $OUT/wall-$arm-r$r.csv
        ;;
      perf)
        timed_run "$BATCH/$arm-r$r" "$OUT/perf-$arm-r$r.stdout" -- \
            env "${ARMENV[@]}" MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c $CORE \
            perf stat -e $PA -o $POUT/$arm-r$r.txt -- \
            $BIN --engine interior --cells "$CELLS" --csv $OUT/perf-$arm-r$r.csv
        ;;
      *) echo "unknown phase $PHASE"; exit 2 ;;
    esac
    echo "DONE $(utc) phase=$PHASE arm=$arm round=$r rc_so_far=$LEG_RC"
  done
  box_guard "$BATCH/r$r"
done
batch_end $BATCH
echo "LEG_WORST_RC=$LEG_RC"
exit $LEG_RC
