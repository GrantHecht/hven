#!/bin/bash
# W5 T8.9r LEG 1 (wall): the U0 27-cell corpus, ONE mode, 3x alternating A/B,
# the lock held by the caller across the WHOLE sequence (11.3).
# $1 = walk|ssn|ipm
R=/home/ghecht/Projects/hven/.scratch/w5t89r
R0=/home/ghecht/Projects/hven
. $R/common.sh
MODE=${1:?walk|ssn|ipm}
LOG=$R/logs/L4-leg1-$MODE.log
OUT=$R/art/raw/leg1/$MODE
mkdir -p $OUT
CELLS="$(sed -n '3p' $R0/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)"
{
echo "PGREP_SEPARATE $(utc) -- taken OUTSIDE the lock, verbatim:"
cat $R/logs/L4-leg1-$MODE.pgrep
box_pgrep
echo "FOREGROUND_START $(utc) mode=$MODE"
echo "recipe: solo, one solve at a time, taskset -c 2, MKL_NUM_THREADS=OMP_NUM_THREADS=1,"
echo "        3x alternating A(102f729)/B(e51a7e0), lock held for the whole sequence."
for r in 1 2 3; do
  echo "--- round $r: box re-check between alternations ---"
  box_pgrep
  for arm in base head; do
    bin=$R/arm-$arm/build/bench/hven_sqp_corpus
    echo "RUN  $(utc) arm=$arm round=$r mode=$MODE"
    env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
        $bin --engine $MODE --cells "$CELLS" --csv $OUT/$arm-r$r.csv > $OUT/$arm-r$r.stdout 2>&1
    fold $?
    echo "DONE $(utc) arm=$arm round=$r rc_so_far=$LEG_RC rows=$(($(grep -vc '^#' $OUT/$arm-r$r.csv)-1))"
  done
done
echo "FOREGROUND_END $(utc) mode=$MODE"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
exit $LEG_RC
