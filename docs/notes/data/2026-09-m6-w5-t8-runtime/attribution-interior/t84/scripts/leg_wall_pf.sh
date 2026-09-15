#!/bin/bash
# W5 T8.9r-attrib3 -- THE WALL LEG'S OWN CONDITION, WITH THE FAULT COUNT ADDED.
# The mem leg measures page faults under `perf stat`, whose event list changes the
# measured child's argv+environ footprint -- and attribution.md section 6 records
# that these cells are bimodal in exactly that footprint. This leg therefore counts
# the faults where the WALL is measured: no perf, the pin's invocation byte for byte,
# and the fault count read from /usr/bin/time's own %R, which changes nothing the
# child sees (the format string is /usr/bin/time's argv, not the child's).
# $1 = round (1..5)
R=/home/ghecht/Projects/hven/.scratch/w5t89ra3
. $R/common.sh
. $R/arms.sh
ROUND=${1:?round}
BATCH="wallpf-r$ROUND"
LOG=$R/logs/F$ROUND-$BATCH.log
mkdir -p $R/art/fcsv $R/art/fraw
TIMEBUF2=$R/snap/time2
timed_run_pf() {
    local tag=$1 red=$2; shift 2
    [ "$1" = "--" ] && shift
    cpu_stat "$tag" pre
    /usr/bin/time -f "CPUTIME_SELF tag=$tag user=%U sys=%S real=%e minf=%R majf=%F" \
        -o "$TIMEBUF2" -- "$@" > "$red" 2>&1
    fold $?
    cpu_stat "$tag" post
    cat "$TIMEBUF2"
}
{
echo "PGREP_SEPARATE $(utc) -- taken OUTSIDE the lock by the caller, verbatim:"
cat $R/logs/$BATCH.pgrep
echo "--- inside the lock from here ---"
echo "PGREP_INLOCK $(utc)"
pgrep -af 'cmake|ninja|ctest|hven_|codex|clang' || echo PGREP_EMPTY
echo "FOREGROUND_START $(utc) wallpf round=$ROUND"
batch_start $BATCH
if [ $((ROUND % 2)) -eq 1 ]; then ORDER="h1:8ae1618 h2:9cebbbe"; else ORDER="h2:9cebbbe h1:8ae1618"; fi
echo "ARM_ORDER round=$ROUND: $ORDER"
for a in $ORDER; do
  NN=${a%%:*}; SHA=${a#*:}
  BIN=$ARMBIN/$SHA/hven_sqp_corpus
  CSV=$R/art/fcsv/f-$NN-r$ROUND.csv
  echo "ARGV_LEN arm=$NN $(printf '%s --engine interior --cells %s --csv %s' $BIN $CELLS $CSV | wc -c)"
  echo "RUN  $(utc) wallpf arm=$NN sha=$SHA round=$ROUND"
  timed_run_pf "$BATCH/$NN" "$R/art/fraw/f-$NN-r$ROUND.stdout" -- \
      env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
      $BIN --engine interior --cells $CELLS --csv $CSV
  echo "DONE $(utc) arm=$NN rc_so_far=$LEG_RC"
  box_guard "$BATCH/$NN-after"
done
batch_end $BATCH
echo "FOREGROUND_END $(utc) wallpf round=$ROUND"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
exit $LEG_RC
