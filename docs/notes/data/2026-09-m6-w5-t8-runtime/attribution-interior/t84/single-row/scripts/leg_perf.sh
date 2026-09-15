#!/bin/bash
# W5 T8.9r-attrib5 -- ONE PERF BATCH = one (pass, round) over the three arms,
# each arm running the SAME twelve SINGLE-ROW interior processes.
#
# Every process writes exactly ONE base row, so the three arms run the SAME
# WORK: the row-set mismatch that forbade an instruction verdict on this leg
# (reading.md §5 (iv), §11) does not exist here, and neither does the warm-up
# row -- every row is its own process and pays its own warm-up, identically at
# every arm.
#
# ARGV LOCK: the three arm directories are the same length (bin/510a4bb,
# bin/9cebbbe, bin/control), and so is every other component, so the child's
# argv byte length is IDENTICAL across arms for the same row. The leg prints it
# per run and the batch asserts the three agree.
#
# $1 = pass (A|B|C)   $2 = round (1..5)
cd / && cd /home/ghecht/Projects/hven || exit 70
R=/home/ghecht/Projects/hven/.scratch/w5t89ra5
. $R/common.sh
PASS=${1:?A|B|C} ROUND=${2:?round}
BATCH="perf$PASS-r$ROUND"
LOG=$R/logs/P$PASS$ROUND-$BATCH.log
PA=instructions:u,branches:u,cycles:u,branch-misses:u,L1-icache-load-misses:u
PB=instructions:u,cycles:u,de_dis_uop_queue_empty_di0:u,op_cache_hit_miss.op_cache_miss:u,op_cache_hit_miss.op_cache_hit:u,ic_fetch_stall.ic_stall_any:u
PC=L1-dcache-load-misses:u,LLC-load-misses:u,dTLB-load-misses:u,page-faults
EV=$PA; [ "$PASS" = B ] && EV=$PB; [ "$PASS" = C ] && EV=$PC
# Pass S -- THE SUBTRAHEND. Same events as pass A, on the smallest cell the
# leg has: hs071_x1_fixed, four variables dense, 0.00018 s of solve inside a
# warm process. A single-row process is dominated by a PER-PROCESS first-call
# cost (MKL's first call and the allocator's first growth) that the leg proper
# pays ONCE for 43 rows and this instrument pays once PER ROW; pass S measures
# that constant at each arm, on a process whose own row costs nothing, so the
# scored rows can be differenced against it.
#
# THIS IS NOT THE INSTRUMENT reading.md 5 (iv) REFUSED. That one differenced a
# CELL PROCESS (where a large F7 solve had already paid the first call) against
# the unconditional row set run alone (where it had not), and nine of its
# differences came out NEGATIVE. Here EVERY measured row is its own process and
# its own FIRST solve, subtrahend included: the two terms are homogeneous.
[ "$PASS" = S ] && EV=$PA
ARMS="510a4bb 9cebbbe control"
ROWS="f7_n1000_bound_physics:MakeParameter f7_n1000_bound_physics:MakeConstraint f7_n1000_bound_physics:RelaxBounds \
f7_n5000_bound_physics:MakeParameter f7_n5000_bound_physics:MakeConstraint f7_n5000_bound_physics:RelaxBounds \
f7_n10000_bound_neutral:MakeParameter f7_n10000_bound_neutral:MakeConstraint f7_n10000_bound_neutral:RelaxBounds \
f7_n20000_bound_neutral:MakeParameter f7_n20000_bound_neutral:MakeConstraint f7_n20000_bound_neutral:RelaxBounds"
[ "$PASS" = S ] && ROWS="hs071_x1_fixed:MakeParameter hs071_x1_fixed:MakeConstraint hs071_x1_fixed:RelaxBounds"
{
echo "PGREP_SEPARATE $(utc) -- taken OUTSIDE the lock by the caller; build/solve processes:"
grep -E 'cmake|ninja|ctest|hven_' $R/logs/$BATCH.pgrep || echo PGREP_EMPTY_BUILD
echo "--- inside the lock from here ---"
echo "PGREP_INLOCK $(utc)"
pgrep -af 'cmake|ninja|ctest|hven_' || echo PGREP_EMPTY_BUILD
echo "FOREGROUND_START $(utc) perf pass=$PASS round=$ROUND"
echo "events: $EV"
echo "ENV_FOOTPRINT bytes=$(env | wc -c) md5=$(env | md5sum | cut -d' ' -f1) pwd=$PWD shlvl=$SHLVL"
# Arm order ROTATES with the round, so no arm always runs first.
set -- $ARMS; N=$#; OFF=$((ROUND-1)); ORDER=""
for i in $(seq 0 $((N-1))); do j=$(( (i + OFF) % N + 1 )); eval "a=\${$j}"; ORDER="$ORDER $a"; done
echo "ARM_ORDER pass=$PASS round=$ROUND:$ORDER"
for A in $ORDER; do
  BIN=$R/bin/$A/hven_sqp_corpus
  OUT=$R/art/$PASS/$A; PRF=$R/art/perf$PASS/$A
  mkdir -p $OUT $PRF
  for rc in $ROWS; do
    CELL=${rc%%:*}; T=${rc#*:}
    CSV=$OUT/$CELL-$T-r$ROUND.csv
    echo "ARGV_LEN arm=$A row=$CELL/$T $(printf '%s --internal-run-one %s --engine interior --treatment %s --internal-out %s' $BIN $CELL $T $CSV | wc -c)"
    echo "RUN  $(utc) perf$PASS arm=$A row=$CELL/$T round=$ROUND"
    env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
        perf stat -e $EV -o $PRF/$CELL-$T-r$ROUND.txt -- \
        $BIN --internal-run-one $CELL --engine interior --treatment $T --internal-out $CSV \
        > /dev/null 2>&1
    fold $?
    n=$(grep -vc '^#' $CSV 2>/dev/null || echo 0)
    k=$(grep -v '^#' $CSV 2>/dev/null | tail -1 | cut -d, -f1)
    if [ "$n" != "2" ] || [ "$k" != "$CELL/$T" ]; then
      echo "ROW CHECK FAILED arm=$A row=$CELL/$T lines=$n key=$k"; fold 6
    fi
  done
done
echo "FOREGROUND_END $(utc) perf pass=$PASS round=$ROUND"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
rc=$LEG_RC; echo "WRAPPER_EXIT=$rc"; exit $rc
