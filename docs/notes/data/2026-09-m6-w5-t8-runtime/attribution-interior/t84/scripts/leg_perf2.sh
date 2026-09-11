#!/bin/bash
# W5 T8.9r-attrib3 -- perf record on the parent and the culprit, THE SAME four-cell
# invocation the wall leg times, three rounds each, arm order alternating. A POINTER,
# not a measurement: it says WHERE the cycles sit, and asserts no wall number.
R=/home/ghecht/Projects/hven/.scratch/w5t89ra3
. $R/common.sh
. $R/arms.sh
ROUND=${1:?round}
BATCH="perfrec-r$ROUND"
LOG=$R/logs/P3-$BATCH.log
mkdir -p $R/perf/rec $R/art/rcsv
{
echo "PGREP_SEPARATE $(utc) -- taken OUTSIDE the lock by the caller, verbatim:"
cat $R/logs/$BATCH.pgrep
echo "--- inside the lock from here ---"
echo "PGREP_INLOCK $(utc)"
pgrep -af 'cmake|ninja|ctest|hven_|codex|clang' || echo PGREP_EMPTY
echo "FOREGROUND_START $(utc) perfrec round=$ROUND"
batch_start $BATCH
if [ $((ROUND % 2)) -eq 1 ]; then ORDER="8ae1618 9cebbbe"; else ORDER="9cebbbe 8ae1618"; fi
echo "ARM_ORDER round=$ROUND: $ORDER"
for SHA in $ORDER; do
  BIN=$ARMBIN/$SHA/hven_sqp_corpus
  echo "RUN  $(utc) perfrec sha=$SHA round=$ROUND"
  timed_run "$BATCH/$SHA" "$R/perf/rec/o-$SHA-r$ROUND.stdout" -- \
      env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
      perf record -e cycles:u -F 997 -o $R/perf/rec/$SHA-r$ROUND.data -- \
      $BIN --engine interior --cells $CELLS --csv $R/art/rcsv/r-$SHA-r$ROUND.csv
  echo "DONE $(utc) sha=$SHA rc_so_far=$LEG_RC"
  box_guard "$BATCH/$SHA-after"
done
batch_end $BATCH
echo "FOREGROUND_END $(utc) perfrec round=$ROUND"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
exit $LEG_RC
