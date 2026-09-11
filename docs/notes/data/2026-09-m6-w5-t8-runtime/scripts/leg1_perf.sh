#!/bin/bash
# W5 T8.9r LEG 1 (perf): pass A and pass B on THREE designated cells per mode,
# each measured as ONE process via --internal-run-one (no fork/exec parent in
# the counts), 3x alternating A/B, same discipline as the wall leg (11.2).
# $1 = walk|ssn|ipm
R=/home/ghecht/Projects/hven/.scratch/w5t89r
. $R/common.sh
MODE=${1:?walk|ssn|ipm}
LOG=$R/logs/L5-leg1perf-$MODE.log
OUT=$R/art/perf/leg1/$MODE
mkdir -p $OUT $R/art/raw/leg1perf/$MODE
PA=instructions:u,branches:u,cycles:u,branch-misses:u,L1-icache-load-misses:u
PB=instructions:u,cycles:u,de_dis_uop_queue_empty_di0:u,op_cache_hit_miss.op_cache_miss:u,op_cache_hit_miss.op_cache_hit:u,ic_fetch_stall.ic_stall_any:u
CELLS="f7_n1000_bound_neutral f7_n5000_bound_neutral f7_n20000_bound_neutral"
{
echo "PGREP_SEPARATE $(utc) -- taken OUTSIDE the lock, verbatim:"
cat $R/logs/L5-leg1perf-$MODE.pgrep
box_pgrep
echo "FOREGROUND_START $(utc) mode=$MODE"
echo "pass A events: $PA"
echo "pass B events: $PB  (Zen 3, exactly six -- a seventh multiplexes)"
for pass in A B; do
  ev=$PA; [ $pass = B ] && ev=$PB
  for r in 1 2 3; do
    echo "--- pass $pass round $r: box re-check between alternations ---"
    box_pgrep
    for arm in base head; do
      bin=$R/arm-$arm/build/bench/hven_sqp_corpus
      for cell in $CELLS; do
        echo "RUN  $(utc) pass=$pass arm=$arm round=$r cell=$cell"
        env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
            perf stat -e $ev -o $OUT/pass$pass-$arm-$cell-r$r.txt -- \
            $bin --internal-run-one $cell --engine $MODE \
                 --internal-out $R/art/raw/leg1perf/$MODE/$pass-$arm-$cell-r$r.csv \
            > /dev/null 2>&1
        fold $?
      done
    done
  done
done
echo "FOREGROUND_END $(utc) mode=$MODE"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
exit $LEG_RC
