#!/bin/bash
# W5 T8.9r-attrib3 -- the memory-behaviour leg: page faults, dTLB and LLC misses and
# front-end stalls on the parent and the culprit, with a BYTE-IDENTICAL control pair
# measuring the floor. Whole-process counts; NO wall number is asserted here.
# $1 = round (1..3)
R=/home/ghecht/Projects/hven/.scratch/w5t89ra3
. $R/common.sh
. $R/arms_mem.sh
ROUND=${1:?round}
BATCH="mem-r$ROUND"
LOG=$R/logs/M$ROUND-$BATCH.log
mkdir -p $R/perf/mem $R/art/mcsv
EV=cycles:u,instructions:u,page-faults,minor-faults,dTLB-load-misses:u,dTLB-store-misses:u,LLC-load-misses:u,LLC-store-misses:u
set -- $ARMS_MEM
N=$#
{
echo "PGREP_SEPARATE $(utc) -- taken OUTSIDE the lock by the caller, verbatim:"
cat $R/logs/$BATCH.pgrep
echo "--- inside the lock from here ---"
echo "PGREP_INLOCK $(utc)"
pgrep -af 'cmake|ninja|ctest|hven_|codex|clang' || echo PGREP_EMPTY
echo "FOREGROUND_START $(utc) mem round=$ROUND"
batch_start $BATCH
OFF=$((ROUND-1)); ORDER=""
for i in $(seq 0 $((N-1))); do j=$(( (i + OFF) % N + 1 )); eval "a=\${$j}"; ORDER="$ORDER $a"; done
echo "ARM_ORDER round=$ROUND:$ORDER"
for a in $ORDER; do
  NN=${a%%:*}; rest=${a#*:}; TASK=${rest%%:*}; SHA=${rest#*:}
  BIN=$ARMBIN/$SHA/hven_sqp_corpus
  echo "RUN  $(utc) mem arm=$NN kind=$TASK sha=$SHA round=$ROUND"
  timed_run "$BATCH/$NN" "$R/perf/mem/o-$NN-r$ROUND.stdout" -- \
      env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
      perf stat -e $EV -o $R/perf/mem/M-$NN-r$ROUND.txt -- \
      $BIN --engine interior --cells $CELLS --csv $R/art/mcsv/m-$NN-r$ROUND.csv
  echo "DONE $(utc) arm=$NN rc_so_far=$LEG_RC"
  box_guard "$BATCH/$NN-after"
done
batch_end $BATCH
echo "FOREGROUND_END $(utc) mem round=$ROUND"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
exit $LEG_RC
