#!/usr/bin/env bash
# W6 T6 -- THE TOP-LEVEL INTERIOR-POINT LEG (bench/ipm_corpus_leg.cpp, driven by
# --engine interior).  TWO ARMS, and the lever is the one that already exists:
#   off = HVEN_LEG_COUNT_CALLBACK unset -- nothing installed, the shipped shape
#   on  = HVEN_LEG_COUNT_CALLBACK=1  -- a counting iteration callback attached
#         to EVERY row's measured solver (bench/ipm_corpus_leg.cpp:638-645)
# THERE IS NO SINK LEVER ON THIS LEG and this task adds none (the brief's
# "measure what exists"); the omission is declared in PROVENANCE.txt.
# $1 = wall|perf   $2 = rounds (wall: 1, perf: 3)
set -o pipefail
source /home/ghecht/Projects/hven/.scratch/w6t6/env.sh
source $F/common.sh
PHASE=${1:?wall|perf}
ROUNDS=${2:-3}
BATCH="ipmleg2-$PHASE"
OUT=$F/art/raw/ipmleg2
POUT=$F/art/perf/ipmleg2
mkdir -p $OUT $POUT
CELLS="$(sed -n '3p' $R/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)"
echo "BATCH $BATCH phase=$PHASE rounds=$ROUNDS"
echo "pin: taskset -c $CORE (SMT sibling $SIB left idle), MKL_NUM_THREADS=OMP_NUM_THREADS=1"
# W6 T6, THE CONFIRMING BATCH. The first batch's round-1 UNATTACHED process read
# +2.1 % instructions over its own rounds 2 and 3 while every one of the six
# processes wrote BYTE-IDENTICAL rows -- a first-process effect, not a
# trajectory. This batch runs ONE UNTIMED, UNRECORDED warm-up process first, so
# no round of it is the first process of the batch, and reports three rounds
# beside the first batch's three. Nothing of the first batch is discarded or
# overwritten: both are in the artifact and the reading states both.
echo "WARMUP $(utc) -- one untimed, unrecorded run; its counters go nowhere"
env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c $CORE \
    $BIN --engine interior --cells "$CELLS" --csv $OUT/warmup.csv > $OUT/warmup.stdout 2>&1
echo "WARMUP_RC=$? (NOT folded: a warm-up is not a measurement)"
batch_start $BATCH
for r in $(seq 1 $ROUNDS); do
  for arm in off on; do
    ENVV=()
    [ $arm = on ] && ENVV=(HVEN_LEG_COUNT_CALLBACK=1)
    echo "RUN  $(utc) phase=$PHASE arm=$arm round=$r"
    case $PHASE in
      wall)
        timed_run "$BATCH/$arm-r$r" "$OUT/wall-$arm-r$r.stdout" -- \
            env "${ENVV[@]}" MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c $CORE \
            $BIN --engine interior --cells "$CELLS" --csv $OUT/wall-$arm-r$r.csv
        ;;
      perf)
        timed_run "$BATCH/$arm-r$r" "$OUT/perf-$arm-r$r.stdout" -- \
            env "${ENVV[@]}" MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c $CORE \
            perf stat -e $PA -o $POUT/$arm-r$r.txt -- \
            $BIN --engine interior --cells "$CELLS" --csv $OUT/perf-$arm-r$r.csv
        ;;
    esac
    echo "DONE $(utc) phase=$PHASE arm=$arm round=$r rc_so_far=$LEG_RC"
  done
  box_guard "$BATCH/r$r"
done
batch_end $BATCH
echo "LEG_WORST_RC=$LEG_RC"
exit $LEG_RC
