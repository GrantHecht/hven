#!/bin/bash
# W5 T8.9r-attrib3 -- ONE EXPERIMENT WALL ROUND = ONE BATCH. Four arms (parent,
# culprit, and the two patched arms), round-robin, the arm order ROTATED by
# (round-1). THIS ASSERTS WALL CLOCK (CLAUDE.md section 7): the fix1 R2 discipline.
# $1 = round (1..5)
R=/home/ghecht/Projects/hven/.scratch/w5t89ra3
. $R/common.sh
. $R/arms_exp.sh
ROUND=${1:?round}
BATCH="xwall-r$ROUND"
LOG=$R/logs/X$ROUND-$BATCH.log
mkdir -p $R/art/xcsv $R/art/xraw
set -- $ARMS_EXP
N=$#
{
echo "PGREP_SEPARATE $(utc) -- taken OUTSIDE the lock by the caller, verbatim:"
cat $R/logs/$BATCH.pgrep
echo "--- inside the lock from here ---"
echo "PGREP_INLOCK $(utc)"
pgrep -af 'cmake|ninja|ctest|hven_|codex|clang' || echo PGREP_EMPTY
echo "FOREGROUND_START $(utc) xwall round=$ROUND"
echo "ENV_FOOTPRINT bytes=$(env | wc -c) md5=$(env | md5sum | cut -d' ' -f1) pwd=$PWD shlvl=$SHLVL"
echo "CELLS=$CELLS"
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
  CSV=$R/art/xcsv/x-$NN-r$ROUND.csv
  echo "ARGV_LEN arm=$NN $(printf '%s --engine interior --cells %s --csv %s' $BIN $CELLS $CSV | wc -c)"
  echo "RUN  $(utc) xwall arm=$NN kind=$TASK bin=$SHA round=$ROUND"
  timed_run "$BATCH/$NN" "$R/art/xraw/x-$NN-r$ROUND.stdout" -- \
      env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
      $BIN --engine interior --cells $CELLS --csv $CSV
  echo "DONE $(utc) arm=$NN rc_so_far=$LEG_RC rows=$(($(grep -vc '^#' $CSV 2>/dev/null || echo 1)-1))"
  box_guard "$BATCH/$NN-after"
done
batch_end $BATCH
echo "FOREGROUND_END $(utc) xwall round=$ROUND"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
exit $LEG_RC
