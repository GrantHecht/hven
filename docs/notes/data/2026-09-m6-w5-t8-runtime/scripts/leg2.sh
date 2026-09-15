#!/bin/bash
# W5 T8.9r fix1 -- LEG 2: the 27 Hock-Schittkowski problems under --repeat N,
# ONE mode, ONE trace variant, ONE PASS (the batch). Pass A carries the leg's
# would-be verdict (instructions/branches); the wall is read only as the paired
# A/B ratio (11.3 (ii)). N is the count calibrated in round 1 and carried
# forward unchanged (declared in PROVENANCE.txt's fix1 block).
# $1 = walk|ssn|ipm   $2 = off|sink   $3 = N   $4 = A|B
R=/home/ghecht/Projects/hven/.scratch/w5t89r
. $R/fix1/common.sh
MODE=${1:?walk|ssn|ipm}
TRACE=${2:?off|sink}
N=${3:?repeat count}
PASS=${4:?A|B}
BATCH="leg2-$MODE-$TRACE-pass$PASS"
LOG=$R/logs2/L6-leg2-$MODE-$TRACE-pass$PASS.log
OUT=$R/art2/raw/leg2/$MODE/$TRACE
POUT=$R/art2/perf/leg2/$MODE/$TRACE
mkdir -p $OUT $POUT $R/logs2
PA=instructions:u,branches:u,cycles:u,branch-misses:u,L1-icache-load-misses:u
PB=instructions:u,cycles:u,de_dis_uop_queue_empty_di0:u,op_cache_hit_miss.op_cache_miss:u,op_cache_hit_miss.op_cache_hit:u,ic_fetch_stall.ic_stall_any:u
ev=$PA; [ $PASS = B ] && ev=$PB
{
echo "PGREP_SEPARATE $(utc) -- taken OUTSIDE the lock by the caller, verbatim:"
cat $R/logs2/$BATCH.pgrep
echo "--- inside the lock from here ---"
echo "PGREP_INLOCK $(utc)"
pgrep -af 'cmake|ninja|ctest|hven_|codex|clang' || echo PGREP_EMPTY
echo "FOREGROUND_START $(utc) mode=$MODE trace=$TRACE N=$N pass=$PASS"
batch_start $BATCH
for r in 1 2 3; do
  for arm in base head; do
    bin=$R/arm-$arm/build/bench/hven_sqp_corpus
    csv=$OUT/$arm-r$r.csv
    [ $PASS = B ] && csv=$OUT/passB-$arm-r$r.csv
    echo "RUN  $(utc) pass=$PASS arm=$arm round=$r"
    timed_run "$BATCH/$arm-r$r" "$OUT/$PASS-$arm-r$r.stdout" -- \
        env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
        perf stat -e $ev -o $POUT/pass$PASS-$arm-r$r.txt -- \
        $bin --hs --engine $MODE --repeat $N --hs-warmup 1 --hs-trace $TRACE --csv $csv
    echo "DONE $(utc) pass=$PASS arm=$arm round=$r rc_so_far=$LEG_RC"
  done
  box_guard "$BATCH/r$r"
done
batch_end $BATCH
echo "FOREGROUND_END $(utc) mode=$MODE trace=$TRACE pass=$PASS"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
exit $LEG_RC
