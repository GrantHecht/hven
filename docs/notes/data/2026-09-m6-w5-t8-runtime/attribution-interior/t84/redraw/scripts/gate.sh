#!/bin/bash
# W5 T8.9r-attrib4 -- THE CORRECTNESS GATE. The WHOLE `--engine interior` leg
# (every row, not the four timed cells) at head___, e1_____ and e2_____, compared
# column by column. UNTIMED: nothing here is quoted as a measurement, and the
# only column allowed to differ is wall_s.
R=/home/ghecht/Projects/hven/.scratch/w5t89ra4
. $R/common.sh
LOG=$R/logs/B2-gate.log
mkdir -p $R/art/gate
{
echo "PGREP_SEPARATE $(utc) -- taken OUTSIDE the lock by the caller, verbatim:"
cat $R/logs/gate.pgrep
echo "--- inside the lock from here ---"
echo "PGREP_INLOCK $(utc)"
pgrep -af 'cmake|ninja|ctest|hven_|codex|clang' || echo PGREP_EMPTY
echo "FOREGROUND_START $(utc) gate"
for NAME in head___ e1_____ e2_____ base___; do
  CSV=$R/art/gate/g-$NAME.csv
  echo "RUN  $(utc) gate arm=$NAME"
  run env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
      $R/bin/$NAME/hven_sqp_corpus --engine interior --cells all --csv $CSV \
      > $R/art/gate/g-$NAME.stdout 2>&1
  echo "ROWS $NAME $(($(grep -vc '^#' $CSV 2>/dev/null || echo 1)-1)) header_cols=$(grep -v '^#' $CSV | head -1 | awk -F, '{print NF}')"
  echo "rc_so_far=$LEG_RC"
done
echo "FOREGROUND_END $(utc) gate"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
exit $LEG_RC
