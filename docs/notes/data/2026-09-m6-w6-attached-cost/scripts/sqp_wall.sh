#!/usr/bin/env bash
# W6 T6 -- THE SQP CORPUS WALL LEG, ONE MODE, ONE ARM, ONE BATCH.
#
# CLAUDE.md section 7's SERIAL RULE: one solve at a time, alone on the box,
# MKL_NUM_THREADS=1, pinned core, the lock held across the batch.  ONE reading
# per arm (the brief), so a batch is one arm; the four arms of a mode are run
# back to back in a00/a10/a01/a11 order, each its own batch under its own lock.
# The reading is INFORMATIONAL: the asserted instrument is instructions.
#
# The CSV this writes is also the COUNTER BYTE-IDENTITY population: 27 cells x
# the 76-column schema, compared attached-vs-unattached by scripts/compare_replay.py.
# $1 = walk|ssn|ipm   $2 = a00|a10|a01|a11
set -o pipefail
source /home/ghecht/Projects/hven/.scratch/w6t6/env.sh
source $F/common.sh
MODE=${1:?walk|ssn|ipm}
ARM=${2:?a00|a10|a01|a11}
case $ARM in
  a00) CB=off;   TR=off  ;;
  a10) CB=count; TR=off  ;;
  a01) CB=off;   TR=sink ;;
  a11) CB=count; TR=sink ;;
  *) echo "unknown arm $ARM"; exit 2 ;;
esac
BATCH="sqpwall-$MODE-$ARM"
OUT=$F/art/raw/sqpwall/$MODE
mkdir -p $OUT
CELLS="$(sed -n '3p' $R/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)"
echo "BATCH $BATCH mode=$MODE arm=$ARM callback=$CB trace=$TR"
echo "pin: taskset -c $CORE (SMT sibling $SIB left idle), MKL_NUM_THREADS=OMP_NUM_THREADS=1"
batch_start $BATCH
echo "RUN  $(utc) mode=$MODE arm=$ARM"
timed_run "$BATCH/$ARM" "$OUT/$ARM.stdout" -- \
    env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c $CORE \
    $BIN --engine $MODE --cells "$CELLS" --csv $OUT/$ARM.csv \
         --callback $CB --trace $TR
echo "DONE $(utc) arm=$ARM rc_so_far=$LEG_RC rows=$(($(grep -vc '^#' $OUT/$ARM.csv)-1))"
batch_end $BATCH
echo "LEG_WORST_RC=$LEG_RC"
exit $LEG_RC
