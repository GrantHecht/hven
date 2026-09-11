#!/bin/bash
# W5 T8.9r-attrib3 -- whole-process perf stat on ALL SIX arms, the SAME invocation
# the wall leg times. This leg has two ZERO CONTROLS (b01->b02 and b05->b06 are
# byte-identical executables), so the instrument's floor is MEASURED here in every
# currency, not assumed -- and the pair under test (b02->b03) is LIKE-FOR-LIKE
# (19 rows at both arms). Wall clock is NOT asserted from this leg.
# $1 = round (1..3)
R=/home/ghecht/Projects/hven/.scratch/w5t89ra3
. $R/common.sh
. $R/arms.sh
ROUND=${1:?round}
BATCH="perfstat-r$ROUND"
LOG=$R/logs/P2-$BATCH.log
mkdir -p $R/perf/stat $R/art/pcsv
EV=instructions:u,cycles:u,branches:u,branch-misses:u,L1-icache-load-misses:u
set -- $ARMS
N=$#
{
echo "PGREP_SEPARATE $(utc) -- taken OUTSIDE the lock by the caller, verbatim:"
cat $R/logs/$BATCH.pgrep
echo "--- inside the lock from here ---"
echo "PGREP_INLOCK $(utc)"
pgrep -af 'cmake|ninja|ctest|hven_|codex|clang' || echo PGREP_EMPTY
echo "FOREGROUND_START $(utc) perfstat round=$ROUND"
batch_start $BATCH
OFF=$((ROUND-1))
ORDER=""
for i in $(seq 0 $((N-1))); do
  j=$(( (i + OFF) % N + 1 )); eval "a=\${$j}"; ORDER="$ORDER $a"
done
echo "ARM_ORDER round=$ROUND:$ORDER"
for a in $ORDER; do
  NN=${a%%:*}; rest=${a#*:}; TASK=${rest%%:*}; SHA=${rest#*:}
  BIN=$ARMBIN/$SHA/hven_sqp_corpus
  CSV=$R/art/pcsv/s-b$NN-r$ROUND.csv
  echo "RUN  $(utc) perfstat arm=b$NN task=$TASK sha=$SHA round=$ROUND"
  timed_run "$BATCH/b$NN" "$R/perf/stat/o-b$NN-r$ROUND.stdout" -- \
      env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
      perf stat -e $EV -o $R/perf/stat/S-b$NN-r$ROUND.txt -- \
      $BIN --engine interior --cells $CELLS --csv $CSV
  echo "DONE $(utc) arm=b$NN rc_so_far=$LEG_RC rows=$(($(grep -vc '^#' $CSV 2>/dev/null || echo 1)-1))"
  box_guard "$BATCH/b$NN-after"
done
batch_end $BATCH
echo "FOREGROUND_END $(utc) perfstat round=$ROUND"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
exit $LEG_RC
