#!/bin/bash
# W5 T8.9r-attrib3 -- perf record/annotate on the culprit and its parent, one big
# cell, user space. THIS DOES NOT ASSERT WALL CLOCK: it is a POINTER at where the
# cycles moved, taken under the same solo recipe so it is taken on the same box
# state as the wall batches.
R=/home/ghecht/Projects/hven/.scratch/w5t89ra3
. $R/common.sh
. $R/arms.sh
BATCH=perf
LOG=$R/logs/P1-perf.log
mkdir -p $R/perf
CELL=f7_n20000_bound_neutral
{
echo "PGREP_SEPARATE $(utc) -- taken OUTSIDE the lock by the caller, verbatim:"
cat $R/logs/perf.pgrep
echo "--- inside the lock from here ---"
echo "PGREP_INLOCK $(utc)"
pgrep -af 'cmake|ninja|ctest|hven_|codex|clang' || echo PGREP_EMPTY
echo "FOREGROUND_START $(utc) perf"
batch_start $BATCH
for SHA in 8ae1618 9cebbbe; do
  BIN=$ARMBIN/$SHA/hven_sqp_corpus
  echo "RUN  $(utc) perf record sha=$SHA cell=$CELL"
  timed_run "$BATCH/$SHA" "$R/perf/rec-$SHA.stdout" -- \
      env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
      perf record -e cycles:u -F 1999 \
        -o $R/perf/$SHA.data -- \
        $BIN --engine interior --cells $CELL --csv $R/perf/p-$SHA.csv
  echo "DONE $(utc) sha=$SHA rc_so_far=$LEG_RC"
  box_guard "$BATCH/$SHA-after"
done
batch_end $BATCH
echo "FOREGROUND_END $(utc) perf"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
exit $LEG_RC
