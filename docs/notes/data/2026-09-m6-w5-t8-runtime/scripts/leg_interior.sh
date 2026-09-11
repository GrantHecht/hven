#!/bin/bash
# W5 T8.9r THE INTERIOR LEG: --engine interior, base arm b9848bf (41 rows) ->
# head e51a7e0 (43 rows, 41 comparable). EACH ARM RUNS ITS OWN HARNESS SOURCE
# (A7 (i)); wall_s brackets the SOLVE only, transcription precedes t0.
# The F7 rows carry the band under leg-1 rules; the HS071/infeasible rows are
# millisecond-scale and --repeat is HS-only, so they are instructions-only at
# N = 1 (A7 (ii)). Runs IN PROCESS -- no fork -- so perf counts exactly the leg.
R=/home/ghecht/Projects/hven/.scratch/w5t89r
R0=/home/ghecht/Projects/hven
. $R/common.sh
LOG=$R/logs/L7-interior.log
OUT=$R/art/raw/interior
POUT=$R/art/perf/interior
mkdir -p $OUT $POUT
CELLS="$(sed -n '3p' $R0/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)"
PA=instructions:u,branches:u,cycles:u,branch-misses:u,L1-icache-load-misses:u
PB=instructions:u,cycles:u,de_dis_uop_queue_empty_di0:u,op_cache_hit_miss.op_cache_miss:u,op_cache_hit_miss.op_cache_hit:u,ic_fetch_stall.ic_stall_any:u
armdir() { [ "$1" = base ] && echo arm-interior-base || echo arm-head; }
{
echo "PGREP_SEPARATE $(utc) -- taken OUTSIDE the lock, verbatim:"
cat $R/logs/L7-interior.pgrep
box_pgrep
echo "FOREGROUND_START $(utc) interior leg"
echo "### the WALL rounds (no perf), 3x alternating"
for r in 1 2 3; do
  echo "--- round $r: box re-check between alternations ---"
  box_pgrep
  for arm in base head; do
    bin=$R/$(armdir $arm)/build/bench/hven_sqp_corpus
    echo "RUN  $(utc) arm=$arm round=$r"
    env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
        $bin --engine interior --cells "$CELLS" --csv $OUT/$arm-r$r.csv > $OUT/$arm-r$r.stdout 2>&1
    fold $?
    echo "DONE $(utc) arm=$arm round=$r rc_so_far=$LEG_RC rows=$(($(grep -vc '^#' $OUT/$arm-r$r.csv)-1))"
  done
done
echo "### the PERF rounds"
for pass in A B; do
  ev=$PA; [ $pass = B ] && ev=$PB
  for r in 1 2 3; do
    echo "--- pass $pass round $r: box re-check between alternations ---"
    box_pgrep
    for arm in base head; do
      bin=$R/$(armdir $arm)/build/bench/hven_sqp_corpus
      echo "RUN  $(utc) pass=$pass arm=$arm round=$r"
      env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
          perf stat -e $ev -o $POUT/pass$pass-$arm-r$r.txt -- \
          $bin --engine interior --cells "$CELLS" --csv $OUT/perf$pass-$arm-r$r.csv \
          > /dev/null 2>&1
      fold $?
      echo "DONE $(utc) pass=$pass arm=$arm round=$r rc_so_far=$LEG_RC"
    done
  done
done
echo "FOREGROUND_END $(utc) interior leg"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
exit $LEG_RC
