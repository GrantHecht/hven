#!/bin/bash
# W5 T8.9r fix1 -- LEG 1 (wall): the U0 27-cell corpus, ONE mode, ONE ROUND of
# the 3x alternating A/B sequence. THE BATCH IS THE ROUND: the lock is held by
# the caller across the whole A/B alternation, and the round is the unit R2's
# idle proof is computed over. (Round 1 ran all three rounds in one invocation;
# the walk mode alone takes 21 minutes, which is longer than this session's
# per-command cap, so the rounds are separate batches -- each one a complete
# alternation under the lock, which is what 11.3 requires.)
# $1 = walk|ssn|ipm   $2 = 1|2|3
R=/home/ghecht/Projects/hven/.scratch/w5t89r
R0=/home/ghecht/Projects/hven
. $R/fix1/common.sh
MODE=${1:?walk|ssn|ipm}
ROUND=${2:?1|2|3}
BATCH="leg1-$MODE-r$ROUND"
LOG=$R/logs2/L4-leg1-$MODE-r$ROUND.log
OUT=$R/art2/raw/leg1/$MODE
mkdir -p $OUT $R/logs2
CELLS="$(sed -n '3p' $R0/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)"
{
echo "PGREP_SEPARATE $(utc) -- taken OUTSIDE the lock by the caller, verbatim:"
cat $R/logs2/$BATCH.pgrep
echo "--- inside the lock from here ---"
echo "PGREP_INLOCK $(utc)"
pgrep -af 'cmake|ninja|ctest|hven_|codex|clang' || echo PGREP_EMPTY
echo "FOREGROUND_START $(utc) mode=$MODE round=$ROUND"
echo "recipe: solo, one solve at a time, taskset -c 2, MKL_NUM_THREADS=OMP_NUM_THREADS=1,"
echo "        A(102f729)/B(e51a7e0) alternation, lock held for the whole batch."
batch_start $BATCH
for arm in base head; do
  bin=$R/arm-$arm/build/bench/hven_sqp_corpus
  echo "RUN  $(utc) arm=$arm round=$ROUND mode=$MODE"
  timed_run "$BATCH/$arm" "$OUT/$arm-r$ROUND.stdout" -- \
      env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
      $bin --engine $MODE --cells "$CELLS" --csv $OUT/$arm-r$ROUND.csv
  echo "DONE $(utc) arm=$arm round=$ROUND rc_so_far=$LEG_RC rows=$(($(grep -vc '^#' $OUT/$arm-r$ROUND.csv)-1))"
  box_guard "$BATCH/after-$arm"
done
batch_end $BATCH
echo "FOREGROUND_END $(utc) mode=$MODE round=$ROUND"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
exit $LEG_RC
