#!/bin/bash
# W5 T8.9r-attrib4 -- E3, TRIGGERED (best corpus recovery +4.47 %, under 80 %).
# (a) WHERE the faults are: `perf record -e page-faults -g --call-graph dwarf` on
#     the head for one large cell, reported as a site list by user stack.
# (b) WHAT the memory system says: dTLB and L1-D misses, base vs head vs E2.
# CO-RUN TOLERANT like the fault pass: counters only; no wall figure is quoted.
R=/home/ghecht/Projects/hven/.scratch/w5t89ra4
. $R/common.sh
. $R/arms.sh
LOG=$R/logs/E3-perf.log
CELL=f7_n10000_bound_neutral
mkdir -p $R/perf/e3 $R/art/e3
{
echo "PGREP_SEPARATE $(utc) -- taken OUTSIDE the lock by the caller, verbatim:"
cat $R/logs/e3.pgrep
echo "--- inside the lock from here ---"
echo "PGREP_INLOCK $(utc)"
pgrep -af 'cmake|ninja|ctest|hven_|codex|clang' || echo PGREP_EMPTY
echo "FOREGROUND_START $(utc) e3"
echo "CO_RUN_TOLERANT yes -- counters and profiles only; no wall figure is quoted"
batch_start e3
echo "=== (a) FAULT SITES, head and base, one cell ($CELL), page-faults -g dwarf ==="
for NN in a2:head___ a1:base___ a4:e2_____; do
  TAG=${NN%%:*}; DIR=${NN#*:}
  echo "RUN  $(utc) perfrecord arm=$TAG bin=$DIR"
  run env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
      perf record -e page-faults -c 200 -g --call-graph dwarf,8192 \
      -o $R/perf/e3/pf-$TAG.data -- \
      $R/bin/$DIR/hven_sqp_corpus --engine interior --cells $CELL \
      --csv $R/art/e3/r-$TAG.csv > $R/art/e3/r-$TAG.stdout 2>&1
  echo "--- report arm=$TAG (callers folded, top 25) ---"
  perf report -i $R/perf/e3/pf-$TAG.data --no-children --stdio -g none --percent-limit 0.5 \
      2>/dev/null | head -40
  echo "--- callers of the top fault sites, arm=$TAG ---"
  perf report -i $R/perf/e3/pf-$TAG.data --children --stdio -g graph,1.0,caller \
      --percent-limit 2.0 2>/dev/null | head -120
  echo "rc_so_far=$LEG_RC"
done
echo "=== (b) dTLB / L1-D, base vs head vs E2, three rounds ==="
for ROUND in 1 2 3; do
  for NN in a1:base___ a2:head___ a4:e2_____; do
    TAG=${NN%%:*}; DIR=${NN#*:}
    echo "RUN  $(utc) perfstat-tlb arm=$TAG round=$ROUND"
    run env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
        perf stat -e dTLB-load-misses,dTLB-store-misses,dTLB-loads,dTLB-stores,L1-dcache-load-misses,L1-dcache-loads,instructions,cycles \
        -x, -o $R/perf/e3/tlb-$TAG-r$ROUND.txt -- \
        $R/bin/$DIR/hven_sqp_corpus --engine interior --cells $CELLS \
        --csv $R/art/e3/t-$TAG-r$ROUND.csv > $R/art/e3/t-$TAG-r$ROUND.stdout 2>&1
    sed 's/^/   /' $R/perf/e3/tlb-$TAG-r$ROUND.txt
  done
done
batch_end e3
echo "FOREGROUND_END $(utc) e3"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
exit $LEG_RC
