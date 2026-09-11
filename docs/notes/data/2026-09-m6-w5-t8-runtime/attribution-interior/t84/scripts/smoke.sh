#!/bin/bash
# W5 T8.9r-attrib3 -- the pre-measurement smoke: every arm accepts the pin's
# invocation, and the ROW ORDER (the R3 positional rule's premise) is the same
# at all six. Untimed; nothing here is quoted as a measurement.
R=/home/ghecht/Projects/hven/.scratch/w5t89ra3
. $R/common.sh
. $R/arms.sh
LOG=$R/logs/B2-smoke.log
mkdir -p $R/art/smoke
{
echo "PGREP_SEPARATE $(utc) -- taken OUTSIDE the lock by the caller, verbatim:"
cat $R/logs/smoke.pgrep
echo "--- inside the lock from here ---"
echo "PGREP_INLOCK $(utc)"
pgrep -af 'cmake|ninja|ctest|hven_|codex|clang' || echo PGREP_EMPTY
echo "FOREGROUND_START $(utc) smoke"
for a in $ARMS; do
  NN=${a%%:*}; rest=${a#*:}; TAG=${rest%%:*}; SHA=${rest#*:}
  BIN=$ARMBIN/$SHA/hven_sqp_corpus
  CSV=$R/art/smoke/s-b$NN.csv
  echo "RUN  $(utc) smoke arm=b$NN tag=$TAG sha=$SHA"
  run env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
      $BIN --engine interior --cells $CELLS --csv $CSV > $R/art/smoke/s-b$NN.stdout 2>&1
  echo "ROWS b$NN $(($(grep -vc '^#' $CSV 2>/dev/null || echo 1)-1))  header_cols=$(grep -v '^#' $CSV | head -1 | awk -F, '{print NF}')"
  echo "FIRST5 b$NN:"
  grep -v '^#' $CSV | head -6 | awk -F, 'NR>1{print "   " NR-1 " " $1}'
  echo "rc_so_far=$LEG_RC"
done
echo "FOREGROUND_END $(utc) smoke"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
exit $LEG_RC
