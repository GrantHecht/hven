#!/usr/bin/env bash
# M7 W0 T2 -- THE UNATTACHED PATH, AS A COMMIT PAIR.
#
# The P-SYM gate asks whether the UNATTACHED path's instruction stream moved.
# P-SYM answers it per symbol; this answers it at runtime, which is the reading
# that matters: the SAME leg, the SAME cells, NOTHING attached, run from the
# binary built at d8ed6b28 and from the binary built at b8db4b98. Three rounds,
# the two arms alternated within a round, behind one untimed warm-up.
set -o pipefail
source /home/ghecht/Projects/hven/.scratch/m7t2/env.sh
source $F/common.sh
ROUNDS=${1:-3}
BATCH="unattached-pair"
OUT=$F/art/raw/unattached
POUT=$F/art/perf/unattached
mkdir -p $OUT $POUT
OLD=$F/psym-old-build/bench/hven_sqp_corpus
NEW=$F/psym-new-build/bench/hven_sqp_corpus
CELLS="$(sed -n '3p' $R/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)"
echo "BATCH $BATCH rounds=$ROUNDS"
echo "old = $OLD  (d8ed6b28, the T1 close)"
echo "new = $NEW  (b8db4b98, this task's HEAD)"
echo "pin: taskset -c $CORE (SMT sibling $SIB left idle), MKL_NUM_THREADS=OMP_NUM_THREADS=1"
echo "WARMUP $(utc) -- one untimed, unrecorded run"
env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c $CORE \
    $NEW --engine interior --cells "$CELLS" --csv $OUT/warmup.csv > $OUT/warmup.stdout 2>&1
echo "WARMUP_RC=$? (NOT folded: a warm-up is not a measurement)"
batch_start $BATCH
for r in $(seq 1 $ROUNDS); do
  for arm in old new; do
    BINX=$OLD; [ $arm = new ] && BINX=$NEW
    echo "RUN  $(utc) arm=$arm round=$r"
    timed_run "$BATCH/$arm-r$r" "$OUT/$arm-r$r.stdout" -- \
        env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c $CORE \
        perf stat -e $PA -o $POUT/$arm-r$r.txt -- \
        $BINX --engine interior --cells "$CELLS" --csv $OUT/$arm-r$r.csv
    echo "DONE $(utc) arm=$arm round=$r rc_so_far=$LEG_RC"
  done
  box_guard "$BATCH/r$r"
done
batch_end $BATCH
echo "LEG_WORST_RC=$LEG_RC"
exit $LEG_RC
