#!/bin/bash
# W5 T8.9r fix1 -- I4: THE PER-CELL INTERIOR INSTRUCTION MEASUREMENT.
#
# WHY A SECOND, "COMMON" PROCESS PER ARM. Every `--engine interior` process runs
# an UNCONDITIONAL row set whichever cell is requested -- the three
# `hs071_x1_fixed` treatments, the cap1 / solve_optimize / warm variants and the
# two `infeas2` rows -- and at the head TWO MORE (`parts2`) that no flag
# suppresses. VERIFIED at dispatch: `--cells f7_n1000_bound_physics` writes 6
# rows at 102f729, 14 at b9848bf and 16 at e51a7e0, and the two extra head rows
# are the `parts2` pair, which attaches to f7_n1000_bound_neutral
# UNCONDITIONALLY rather than to the requested cell. So a bare per-cell process
# is NOT like-for-like across the arms, and the dispatch's premise that it would
# be is corrected here.
#
# The instrument that IS like-for-like: `--cells hs071_x1_fixed` is accepted and
# "adds nothing" (bench_corpus.cpp's plan_interior_cells), so that process is
# EXACTLY the unconditional set -- 3 rows at 102f729, 11 at b9848bf, 13 at the
# head. Differencing a cell's process against it on the SAME ARM cancels the
# unconditional set (the head's `parts2` rows included) and process start-up,
# and leaves that cell's own three rows. The comparator does the subtraction.
# $1 = 1|2|3 (the round = the batch)
R=/home/ghecht/Projects/hven/.scratch/w5t89r
. $R/fix1/common.sh
ROUND=${1:?1|2|3}
BATCH="interior-cells-r$ROUND"
LOG=$R/logs2/L8-interior-cells-r$ROUND.log
POUT=$R/art2/perf/interior_cells
OUT=$R/art2/raw/interior_cells
mkdir -p $POUT $OUT $R/logs2
PA=instructions:u,branches:u,cycles:u,branch-misses:u,L1-icache-load-misses:u
CELLS="hs071_x1_fixed f7_n1000_bound_neutral f7_n1000_bound_physics f7_n2000_bound_neutral f7_n2000_bound_physics f7_n5000_bound_neutral f7_n5000_bound_physics f7_n10000_bound_neutral f7_n10000_bound_physics f7_n20000_bound_neutral f7_n20000_bound_physics"
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
echo "FOREGROUND_START $(utc) interior per-cell perf round=$ROUND"
echo "pass A events: $PA"
batch_start $BATCH
for cell in $CELLS; do
  mkdir -p $POUT/$cell $OUT/$cell
  for arm in arm102 armb98 head; do
    bin=$R/$(armdir $arm)/build/bench/hven_sqp_corpus
    echo "RUN  $(utc) cell=$cell arm=$arm round=$ROUND"
    timed_run "$BATCH/$cell-$arm" /dev/null -- \
        env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
        perf stat -e $PA -o $POUT/$cell/passA-$arm-r$ROUND.txt -- \
        $bin --engine interior --cells $cell --csv $OUT/$cell/$arm-r$ROUND.csv
    echo "DONE $(utc) cell=$cell arm=$arm round=$ROUND rc_so_far=$LEG_RC rows=$(($(grep -vc '^#' $OUT/$cell/$arm-r$ROUND.csv)-1))"
  done
  box_guard "$BATCH/$cell"
done
batch_end $BATCH
echo "FOREGROUND_END $(utc) interior per-cell perf round=$ROUND"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
exit $LEG_RC
