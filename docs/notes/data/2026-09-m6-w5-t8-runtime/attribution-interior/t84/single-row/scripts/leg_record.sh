#!/bin/bash
# W5 T8.9r-attrib5 -- THE PER-SYMBOL INSTRUCTION MEASUREMENT.
#
# perf stat counts the PROCESS, and a single-row interior process is dominated
# by MKL's dsecnd() first-call clock calibration (src/drivers/solver_init.cpp:27,
# reached from interior_point_solver.cpp:798) -- a WALL-TIMED busy-wait whose
# instruction count measures how fast that loop's own code runs, not the
# solver's work. This leg samples instructions:u per symbol so the calibration
# can be named and set aside, and the row's OWN instructions read directly.
# $1 = round
cd / && cd /home/ghecht/Projects/hven || exit 70
R=/home/ghecht/Projects/hven/.scratch/w5t89ra5
. $R/common.sh
ROUND=${1:?round}
LOG=$R/logs/D$ROUND-record-r$ROUND.log
ROWS="f7_n20000_bound_neutral:MakeParameter f7_n20000_bound_neutral:MakeConstraint f7_n1000_bound_physics:MakeConstraint"
PERIOD=2000000
{
echo "PGREP_SEPARATE $(utc) -- taken OUTSIDE the lock; build/solve processes:"
grep -E 'cmake|ninja|ctest|hven_' $R/logs/record-r$ROUND.pgrep || echo PGREP_EMPTY_BUILD
echo "--- inside the lock from here ---"
echo "PGREP_INLOCK $(utc)"
pgrep -af 'cmake|ninja|ctest|hven_' || echo PGREP_EMPTY_BUILD
echo "FOREGROUND_START $(utc) record round=$ROUND"
echo "event: instructions:u  period: $PERIOD"
for A in 510a4bb 9cebbbe control; do
  for rc in $ROWS; do
    CELL=${rc%%:*}; T=${rc#*:}
    D=$R/perfrec/$A; mkdir -p $D
    echo "RUN  $(utc) record arm=$A row=$CELL/$T round=$ROUND"
    env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
      perf record -q -e instructions:u -c $PERIOD -o $D/$CELL-$T-r$ROUND.data -- \
      $R/bin/$A/hven_sqp_corpus --internal-run-one $CELL --engine interior \
        --treatment $T --internal-out $R/art/D/$A-$CELL-$T-r$ROUND.csv > /dev/null 2>&1
    fold $?
  done
done
echo "FOREGROUND_END $(utc) record round=$ROUND"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
rc=$LEG_RC; echo "WRAPPER_EXIT=$rc"; exit $rc
