#!/bin/bash
# W5 T8.9r LEG 2: the 27 Hock-Schittkowski problems under --repeat N, ONE mode,
# ONE trace variant. Pass A carries the verdict (instructions/branches); the
# wall is read only as the paired A/B ratio of the CSV's per-cell medians
# (11.3 (ii)). Pass B explains placement. 3x alternating A/B per pass.
# $1 = walk|ssn|ipm   $2 = off|sink   $3 = N
R=/home/ghecht/Projects/hven/.scratch/w5t89r
. $R/common.sh
MODE=${1:?walk|ssn|ipm}
TRACE=${2:?off|sink}
N=${3:?repeat count}
LOG=$R/logs/L6-leg2-$MODE-$TRACE.log
OUT=$R/art/raw/leg2/$MODE/$TRACE
POUT=$R/art/perf/leg2/$MODE/$TRACE
mkdir -p $OUT $POUT
PA=instructions:u,branches:u,cycles:u,branch-misses:u,L1-icache-load-misses:u
PB=instructions:u,cycles:u,de_dis_uop_queue_empty_di0:u,op_cache_hit_miss.op_cache_miss:u,op_cache_hit_miss.op_cache_hit:u,ic_fetch_stall.ic_stall_any:u
{
echo "PGREP_SEPARATE $(utc) -- taken OUTSIDE the lock, verbatim:"
cat $R/logs/L6-leg2-$MODE-$TRACE.pgrep
box_pgrep
echo "FOREGROUND_START $(utc) mode=$MODE trace=$TRACE N=$N"
for pass in A B; do
  ev=$PA; [ $pass = B ] && ev=$PB
  for r in 1 2 3; do
    echo "--- pass $pass round $r: box re-check between alternations ---"
    box_pgrep
    for arm in base head; do
      bin=$R/arm-$arm/build/bench/hven_sqp_corpus
      csv=$OUT/$arm-r$r.csv
      [ $pass = B ] && csv=$OUT/passB-$arm-r$r.csv
      echo "RUN  $(utc) pass=$pass arm=$arm round=$r"
      env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
          perf stat -e $ev -o $POUT/pass$pass-$arm-r$r.txt -- \
          $bin --hs --engine $MODE --repeat $N --hs-warmup 1 --hs-trace $TRACE --csv $csv \
          > $OUT/$pass-$arm-r$r.stdout 2>&1
      fold $?
      echo "DONE $(utc) pass=$pass arm=$arm round=$r rc_so_far=$LEG_RC"
    done
  done
done
echo "FOREGROUND_END $(utc) mode=$MODE trace=$TRACE"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
exit $LEG_RC
