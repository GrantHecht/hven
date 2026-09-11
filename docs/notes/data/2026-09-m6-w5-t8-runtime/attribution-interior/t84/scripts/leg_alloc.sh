#!/bin/bash
# W5 T8.9r-attrib3 -- EXPERIMENT 4: the allocator intervention, applied IDENTICALLY to
# the parent and the culprit, in the WALL LEG'S OWN CONDITION (no perf; the fault count
# from /usr/bin/time's %R, which changes nothing the child sees).
#   default   -- the pin's environment
#   bigthresh -- glibc told to stop handing large blocks back to the kernel
#                (MALLOC_MMAP_THRESHOLD_ / MALLOC_TRIM_THRESHOLD_ / MALLOC_TOP_PAD_)
# If the culprit's step is carried by re-faulting freshly-mapped pages, the step must
# COLLAPSE under `bigthresh` while both arms keep doing the same arithmetic.
# The wall numbers here are quoted under a DECLARED non-pin environment and are read
# as a PAIRED RATIO within each mode only.
# $1 = round (1..5)
R=/home/ghecht/Projects/hven/.scratch/w5t89ra3
. $R/common.sh
. $R/arms.sh
ROUND=${1:?round}
BATCH="alloc-r$ROUND"
LOG=$R/logs/A$ROUND-$BATCH.log
mkdir -p $R/art/acsv $R/art/araw
TIMEBUF2=$R/snap/time3
BIG="MALLOC_MMAP_THRESHOLD_=1073741824 MALLOC_TRIM_THRESHOLD_=1073741824 MALLOC_TOP_PAD_=67108864"
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
echo "FOREGROUND_START $(utc) alloc round=$ROUND"
batch_start $BATCH
# four arms: (default|bigthresh) x (parent|culprit), order rotated by round
SET="d1:default:8ae1618 d2:default:9cebbbe b1:bigthr_:8ae1618 b2:bigthr_:9cebbbe"
set -- $SET; N=$#; OFF=$(((ROUND-1)%N)); ORDER=""
for i in $(seq 0 $((N-1))); do j=$(( (i + OFF) % N + 1 )); eval "a=\${$j}"; ORDER="$ORDER $a"; done
echo "ARM_ORDER round=$ROUND:$ORDER"
for a in $ORDER; do
  NN=${a%%:*}; rest=${a#*:}; MODE=${rest%%:*}; SHA=${rest#*:}
  BIN=$ARMBIN/$SHA/hven_sqp_corpus
  CSV=$R/art/acsv/a-$NN-r$ROUND.csv
  if [ "$MODE" = "default" ]; then EXTRA=""; else EXTRA="$BIG"; fi
  echo "ARGV_LEN arm=$NN $(printf '%s --engine interior --cells %s --csv %s' $BIN $CELLS $CSV | wc -c)"
  echo "RUN  $(utc) alloc arm=$NN mode=$MODE sha=$SHA round=$ROUND"
  timed_run_pf "$BATCH/$NN" "$R/art/araw/a-$NN-r$ROUND.stdout" -- \
      env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 $EXTRA taskset -c 2 \
      $BIN --engine interior --cells $CELLS --csv $CSV
  echo "DONE $(utc) arm=$NN rc_so_far=$LEG_RC"
  box_guard "$BATCH/$NN-after"
done
batch_end $BATCH
echo "FOREGROUND_END $(utc) alloc round=$ROUND"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
exit $LEG_RC
