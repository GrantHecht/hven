#!/bin/bash
# W5 T8.9r-attrib4 -- THE PAGE-FAULT PASS. Declared CO-RUN TOLERANT
# (predeclaration.txt): it asserts a deterministic per-process counter, not wall
# clock, and its wall column is NOT quoted. Two readings per arm per round:
#   (a) `perf stat -e page-faults,minor-faults` -- the counter under perf;
#   (b) `/usr/bin/time %R` with NO perf attached -- the same counter in the wall
#       pin's own invocation, because a perf event list changes the measured
#       child's argv+environ footprint (attribution.md section 6).
# $1 = round
R=/home/ghecht/Projects/hven/.scratch/w5t89ra4
. $R/common.sh
. $R/arms.sh
ROUND=${1:?round}
BATCH="pf-r$ROUND"
LOG=$R/logs/P$ROUND-$BATCH.log
mkdir -p $R/art/pf $R/perf
set -- $ARMS
N=$#
{
echo "PGREP_SEPARATE $(utc) -- taken OUTSIDE the lock by the caller, verbatim:"
cat $R/logs/$BATCH.pgrep
echo "--- inside the lock from here ---"
echo "PGREP_INLOCK $(utc)"
pgrep -af 'cmake|ninja|ctest|hven_|codex|clang' || echo PGREP_EMPTY
echo "FOREGROUND_START $(utc) pf round=$ROUND"
echo "CO_RUN_TOLERANT yes -- counters only; the wall column of this batch is not quoted"
echo "CELLS=$CELLS"
batch_start $BATCH
OFF=$((ROUND-1))
ORDER=""
for i in $(seq 0 $((N-1))); do
  j=$(( (i + OFF) % N + 1 )); eval "a=\${$j}"; ORDER="$ORDER $a"
done
echo "ARM_ORDER round=$ROUND:$ORDER"
for a in $ORDER; do
  NN=${a%%:*}; rest=${a#*:}; TAG=${rest%%:*}; DIR=${rest#*:}
  BIN=$ARMBIN/$DIR/hven_sqp_corpus
  echo "RUN  $(utc) perfstat arm=$NN kind=$TAG bin=$DIR round=$ROUND"
  run env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
      perf stat -e page-faults,minor-faults,major-faults -x, \
      -o $R/perf/pf-$NN-r$ROUND.txt -- \
      $BIN --engine interior --cells $CELLS --csv $R/art/pf/p-$NN-r$ROUND.csv \
      > $R/art/pf/p-$NN-r$ROUND.stdout 2>&1
  echo "PERFSTAT $NN r$ROUND:"; sed 's/^/   /' $R/perf/pf-$NN-r$ROUND.txt
  echo "RUN  $(utc) timeR arm=$NN (no perf attached)"
  run env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
      /usr/bin/time -f "TIMER_FAULTS arm=$NN round=$ROUND minor=%R major=%F user=%U sys=%S real=%e maxrss=%M" \
      -o $R/art/pf/t-$NN-r$ROUND.txt -- \
      $BIN --engine interior --cells $CELLS --csv $R/art/pf/q-$NN-r$ROUND.csv \
      > $R/art/pf/q-$NN-r$ROUND.stdout 2>&1
  cat $R/art/pf/t-$NN-r$ROUND.txt
  echo "rc_so_far=$LEG_RC"
done
batch_end $BATCH
echo "FOREGROUND_END $(utc) pf round=$ROUND"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
exit $LEG_RC
