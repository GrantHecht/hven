#!/bin/bash
# W5 T8.9r fix1 -- LEG 1 (perf): pass A and pass B on THREE designated cells per
# mode, each measured as ONE process via --internal-run-one, 3x alternating A/B,
# the same R2 discipline as the wall leg (11.2). ONE BATCH.
# $1 = walk|ssn|ipm
R=/home/ghecht/Projects/hven/.scratch/w5t89r
. $R/fix1/common.sh
MODE=${1:?walk|ssn|ipm}
BATCH="leg1perf-$MODE"
LOG=$R/logs2/L5-leg1perf-$MODE.log
OUT=$R/art2/perf/leg1/$MODE
mkdir -p $OUT $R/art2/raw/leg1perf/$MODE $R/logs2
PA=instructions:u,branches:u,cycles:u,branch-misses:u,L1-icache-load-misses:u
PB=instructions:u,cycles:u,de_dis_uop_queue_empty_di0:u,op_cache_hit_miss.op_cache_miss:u,op_cache_hit_miss.op_cache_hit:u,ic_fetch_stall.ic_stall_any:u
CELLS="f7_n1000_bound_neutral f7_n5000_bound_neutral f7_n20000_bound_neutral"
{
echo "PGREP_SEPARATE $(utc) -- taken OUTSIDE the lock by the caller, verbatim:"
cat $R/logs2/$BATCH.pgrep
echo "--- inside the lock from here ---"
echo "PGREP_INLOCK $(utc)"
pgrep -af 'cmake|ninja|ctest|hven_|codex|clang' || echo PGREP_EMPTY
echo "FOREGROUND_START $(utc) mode=$MODE"
echo "pass A events: $PA"
echo "pass B events: $PB  (Zen 3, exactly six -- a seventh multiplexes)"
batch_start $BATCH
for pass in A B; do
  ev=$PA; [ $pass = B ] && ev=$PB
  for r in 1 2 3; do
    for arm in base head; do
      bin=$R/arm-$arm/build/bench/hven_sqp_corpus
      for cell in $CELLS; do
        echo "RUN  $(utc) pass=$pass arm=$arm round=$r cell=$cell"
        timed_run "$BATCH/pass$pass-$arm-$cell-r$r" /dev/null -- \
            env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
            perf stat -e $ev -o $OUT/pass$pass-$arm-$cell-r$r.txt -- \
            $bin --internal-run-one $cell --engine $MODE \
                 --internal-out $R/art2/raw/leg1perf/$MODE/$pass-$arm-$cell-r$r.csv
      done
    done
    box_guard "$BATCH/pass$pass-r$r"
  done
done
batch_end $BATCH
echo "FOREGROUND_END $(utc) mode=$MODE"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
exit $LEG_RC
