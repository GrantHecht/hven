#!/usr/bin/env bash
# W6 T6 -- THE HS LEG'S SINK ARM, RE-READ ONCE (the registration, M6 ledger
# :4455-4457: "the HS leg MOVED FASTER in five of six combinations (0.975-0.993),
# informational, below its own 0.22 % instrument floor").  Re-read, NOT
# re-instrumented: the same --hs-trace off|sink lever, the same binary, read on
# INSTRUCTIONS this time, which is the instrument the wall reading was below.
#
# --repeat 200: at --repeat 1 an HS process is 50-80 ms and its instruction count
# is dominated by process start and MKL's first-call init, which no arm changes;
# 200 puts the solves at ~95 % of the process and is still seconds-scale.
# Calibrated by measurement, stamped in PROVENANCE.txt, IDENTICAL across arms.
# $1 = wall|perf   $2 = rounds
set -o pipefail
source /home/ghecht/Projects/hven/.scratch/w6t6/env.sh
source $F/common.sh
PHASE=${1:?wall|perf}
ROUNDS=${2:-3}
REPEAT=200
BATCH="hs-$PHASE"
OUT=$F/art/raw/hs
POUT=$F/art/perf/hs
mkdir -p $OUT $POUT
echo "BATCH $BATCH phase=$PHASE rounds=$ROUNDS repeat=$REPEAT warmup=1"
echo "pin: taskset -c $CORE (SMT sibling $SIB left idle), MKL_NUM_THREADS=OMP_NUM_THREADS=1"
batch_start $BATCH
for r in $(seq 1 $ROUNDS); do
  for mode in walk ssn ipm; do
    for arm in off sink; do
      echo "RUN  $(utc) phase=$PHASE mode=$mode arm=$arm round=$r"
      case $PHASE in
        wall)
          timed_run "$BATCH/$mode-$arm-r$r" "$OUT/wall-$mode-$arm-r$r.stdout" -- \
              env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c $CORE \
              $BIN --hs --engine $mode --csv $OUT/wall-$mode-$arm-r$r.csv \
                   --hs-trace $arm --repeat $REPEAT --hs-warmup 1
          ;;
        perf)
          timed_run "$BATCH/$mode-$arm-r$r" "$OUT/perf-$mode-$arm-r$r.stdout" -- \
              env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c $CORE \
              perf stat -e $PA -o $POUT/$mode-$arm-r$r.txt -- \
              $BIN --hs --engine $mode --csv $OUT/perf-$mode-$arm-r$r.csv \
                   --hs-trace $arm --repeat $REPEAT --hs-warmup 1
          ;;
      esac
      echo "DONE $(utc) mode=$mode arm=$arm round=$r rc_so_far=$LEG_RC"
    done
  done
  box_guard "$BATCH/r$r"
done
batch_end $BATCH
echo "LEG_WORST_RC=$LEG_RC"
exit $LEG_RC
