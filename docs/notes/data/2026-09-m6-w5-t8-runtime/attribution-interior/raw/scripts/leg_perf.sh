#!/bin/bash
# W5 T8.9r-attrib2 -- ONE PERF ROUND = ONE BATCH. `perf stat` on the SAME
# invocation the wall leg times, at every arm named. Whole-process counts:
# LIKE-FOR-LIKE only between consecutive arms whose ROW SET did not change --
# the leg records every arm's row count and the comparator classifies only
# those pairs (brief section 1). Instructions are co-run tolerant, but the
# batch still runs under the lock with the markers and the R2 brackets.
# $1 = pass (A|B)   $2 = round   $3 = arm NN list, space separated ("all")
R=/home/ghecht/Projects/hven/.scratch/w5t89ra2
. $R/common.sh
. $R/arms.sh
PASS=${1:?A|B} ROUND=${2:?round} WANT=${3:-all}
BATCH="perf$PASS-r$ROUND"
LOG=$R/logs/P$PASS$ROUND-$BATCH.log
PA=instructions:u,branches:u,cycles:u,branch-misses:u,L1-icache-load-misses:u
PB=instructions:u,cycles:u,de_dis_uop_queue_empty_di0:u,op_cache_hit_miss.op_cache_miss:u,op_cache_hit_miss.op_cache_hit:u,ic_fetch_stall.ic_stall_any:u
EV=$PA; [ "$PASS" = B ] && EV=$PB
mkdir -p $R/art/csv $R/art/raw $R/art/perf
set -- $ARMS
N=$#
{
echo "PGREP_SEPARATE $(utc) -- taken OUTSIDE the lock by the caller, verbatim:"
cat $R/logs/$BATCH.pgrep
echo "--- inside the lock from here ---"
echo "PGREP_INLOCK $(utc)"
pgrep -af 'cmake|ninja|ctest|hven_|codex|clang' || echo PGREP_EMPTY
echo "FOREGROUND_START $(utc) perf pass=$PASS round=$ROUND arms=$WANT"
echo "events: $EV"
echo "ENV_FOOTPRINT bytes=$(env | wc -c) md5=$(env | md5sum | cut -d' ' -f1) pwd=$PWD shlvl=$SHLVL"
batch_start $BATCH
OFF=$((ROUND-1))
ORDER=""
for i in $(seq 0 $((N-1))); do
  j=$(( (i + OFF) % N + 1 ))
  eval "a=\${$j}"
  nn=${a%%:*}
  case "$WANT" in
    all) ORDER="$ORDER $a" ;;
    *) case " $WANT " in *" $nn "*) ORDER="$ORDER $a" ;; esac ;;
  esac
done
echo "ARM_ORDER pass=$PASS round=$ROUND:$ORDER"
for a in $ORDER; do
  NN=${a%%:*}; rest=${a#*:}; TASK=${rest%%:*}; SHA=${rest#*:}
  BIN=$ARMBIN/$SHA/hven_sqp_corpus
  CSV=$R/art/csv/$PASS-a$NN-r$ROUND.csv
  echo "ARGV_LEN arm=$NN $(printf '%s --engine interior --cells %s --csv %s' $BIN $CELLS $CSV | wc -c)"
  echo "RUN  $(utc) perf$PASS arm=$NN task=$TASK sha=$SHA round=$ROUND"
  timed_run "$BATCH/a$NN" "$R/art/raw/$PASS-a$NN-r$ROUND.stdout" -- \
      env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
      perf stat -e $EV -o $R/art/perf/$PASS-a$NN-r$ROUND.txt -- \
      $BIN --engine interior --cells $CELLS --csv $CSV
  echo "DONE $(utc) arm=$NN rc_so_far=$LEG_RC rows=$(($(grep -vc '^#' $CSV 2>/dev/null || echo 1)-1))"
  box_guard "$BATCH/a$NN-after"
done
batch_end $BATCH
echo "FOREGROUND_END $(utc) perf pass=$PASS round=$ROUND"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
exit $LEG_RC
