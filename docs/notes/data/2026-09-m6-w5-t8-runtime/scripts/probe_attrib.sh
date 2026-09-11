#!/bin/bash
# W5 T8.9r ATTRIBUTION PROBE (not a protocol leg; an aid to the classification
# 11.1.1 sends to the owner). Pass A said instructions are UP outside the 1e-4
# identity band on every leg-1 perf cell. `perf stat` counts the WHOLE PROCESS;
# the wall band is measured around the SOLVE ALONE (corpus_cells.h timed_row).
# So the process carries work the wall does not: program start, model
# construction, the KKT gate, the CSV row.
#
# --dump-qp runs start + model construction + the cell's FIRST QP build + the
# dump write, and SOLVES NOTHING on a kNeutral cell. Its delta is therefore an
# estimate of the NON-SOLVE share of the leg-1 delta. Same discipline as the
# legs: solo, pinned, threads 1, 3x alternating.
R=/home/ghecht/Projects/hven/.scratch/w5t89r
. $R/common.sh
LOG=$R/logs/L7b-attrib-probe.log
OUT=$R/art/perf/attribution
mkdir -p $OUT $R/scratch-dumps
PA=instructions:u,branches:u,cycles:u,branch-misses:u,L1-icache-load-misses:u
{
echo "PGREP_SEPARATE $(utc) -- taken OUTSIDE the lock, verbatim:"
cat $R/logs/L7b-attrib-probe.pgrep
box_pgrep
echo "FOREGROUND_START $(utc) attribution probe"
for r in 1 2 3; do
  echo "--- round $r ---"
  box_pgrep
  for arm in base head; do
    bin=$R/arm-$arm/build/bench/hven_sqp_corpus
    for cell in f7_n1000_bound_neutral f7_n20000_bound_neutral; do
      echo "RUN  $(utc) arm=$arm round=$r cell=$cell"
      env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
          perf stat -e $PA -o $OUT/dumpqp-$arm-$cell-r$r.txt -- \
          $bin --dump-qp $cell --dump-qp-out $R/scratch-dumps/$arm-$cell.txt \
          > /dev/null 2>&1
      fold $?
    done
  done
done
echo "FOREGROUND_END $(utc) attribution probe"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
exit $LEG_RC
