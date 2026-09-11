#!/bin/bash
# W5 T8.9r fix1 -- THE INTERIOR LEG, THREE ARMS (settler ruling R4).
#
#   arm102  102f729  the post-T7 head. The leg EXISTS here (astra I2:
#                    149f29b is an ancestor of 102f729): 33 rows, the
#                    19-column schema. This arm is the one that measures the
#                    TOP-LEVEL IPM across the whole of group 1.
#   armb98  b9848bf  the T8.8 code head, 41 rows -- round 1's base arm, kept.
#   head    e51a7e0  the group-1 head, 43 rows.
#
# EACH ARM RUNS ITS OWN HARNESS SOURCE (A7 (i)); `wall_s` brackets the SOLVE
# only (bench/ipm_corpus_leg.cpp -- transcription precedes t0).
# $1 = wall|perfA|perfB   (the batch)
R=/home/ghecht/Projects/hven/.scratch/w5t89r
R0=/home/ghecht/Projects/hven
. $R/fix1/common.sh
PHASE=${1:?wall|perfA|perfB}
BATCH="interior-$PHASE"
LOG=$R/logs2/L7-interior-$PHASE.log
OUT=$R/art2/raw/interior
POUT=$R/art2/perf/interior
mkdir -p $OUT $POUT $R/logs2
CELLS="$(sed -n '3p' $R0/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)"
PA=instructions:u,branches:u,cycles:u,branch-misses:u,L1-icache-load-misses:u
PB=instructions:u,cycles:u,de_dis_uop_queue_empty_di0:u,op_cache_hit_miss.op_cache_miss:u,op_cache_hit_miss.op_cache_hit:u,ic_fetch_stall.ic_stall_any:u
armdir() {
  case "$1" in
    arm102) echo arm-base ;;
    armb98) echo arm-interior-base ;;
    head)   echo arm-head ;;
  esac
}
{
echo "PGREP_SEPARATE $(utc) -- taken OUTSIDE the lock by the caller, verbatim:"
cat $R/logs2/$BATCH.pgrep
echo "--- inside the lock from here ---"
echo "PGREP_INLOCK $(utc)"
pgrep -af 'cmake|ninja|ctest|hven_|codex|clang' || echo PGREP_EMPTY
echo "FOREGROUND_START $(utc) interior leg phase=$PHASE"
batch_start $BATCH
for r in 1 2 3; do
  for arm in arm102 armb98 head; do
    bin=$R/$(armdir $arm)/build/bench/hven_sqp_corpus
    echo "RUN  $(utc) phase=$PHASE arm=$arm round=$r"
    case $PHASE in
      wall)
        timed_run "$BATCH/$arm-r$r" "$OUT/$arm-r$r.stdout" -- \
            env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
            $bin --engine interior --cells "$CELLS" --csv $OUT/$arm-r$r.csv
        echo "DONE $(utc) arm=$arm round=$r rc_so_far=$LEG_RC rows=$(($(grep -vc '^#' $OUT/$arm-r$r.csv)-1))"
        ;;
      perfA|perfB)
        ev=$PA; [ $PHASE = perfB ] && ev=$PB
        p=$(echo $PHASE | sed 's/perf/pass/')
        timed_run "$BATCH/$arm-r$r" /dev/null -- \
            env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
            perf stat -e $ev -o $POUT/$p-$arm-r$r.txt -- \
            $bin --engine interior --cells "$CELLS" --csv $OUT/$p-$arm-r$r.csv
        echo "DONE $(utc) phase=$PHASE arm=$arm round=$r rc_so_far=$LEG_RC"
        ;;
    esac
  done
  box_guard "$BATCH/r$r"
done
batch_end $BATCH
echo "FOREGROUND_END $(utc) interior leg phase=$PHASE"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
exit $LEG_RC
