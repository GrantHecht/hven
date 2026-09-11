#!/bin/bash
# W5 T8.9r-attrib3 -- SCREENING probe (not a measurement, not an experiment): does the
# culprit's extra minor-fault count survive when glibc is told to stop returning large
# blocks to the kernel? Page faults and sys time only; no wall number is asserted and
# the environment is deliberately NOT the pin's.
R=/home/ghecht/Projects/hven/.scratch/w5t89ra3
. $R/common.sh
. $R/arms_mem.sh
LOG=$R/logs/S1-screen-malloc.log
mkdir -p $R/perf/screen $R/art/scsv
EV=page-faults,minor-faults,cycles:u,instructions:u
{
echo "PGREP_SEPARATE $(utc)"; cat $R/logs/screen.pgrep
echo "--- inside the lock from here ---"
echo "FOREGROUND_START $(utc) screen"
batch_start screen
for MODE in default bigthresh; do
  for a in g2:8ae1618 g3:9cebbbe; do
    NN=${a%%:*}; SHA=${a#*:}
    if [ $MODE = default ]; then EXTRA=""; else EXTRA="MALLOC_MMAP_THRESHOLD_=1073741824 MALLOC_TRIM_THRESHOLD_=1073741824 MALLOC_TOP_PAD_=67108864"; fi
    echo "RUN  $(utc) screen mode=$MODE arm=$NN sha=$SHA  extra=[$EXTRA]"
    timed_run "screen/$MODE-$NN" "$R/perf/screen/o-$MODE-$NN.stdout" -- \
        env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 $EXTRA taskset -c 2 \
        perf stat -e $EV -o $R/perf/screen/S-$MODE-$NN.txt -- \
        $ARMBIN/$SHA/hven_sqp_corpus --engine interior --cells $CELLS \
          --csv $R/art/scsv/s-$MODE-$NN.csv
    echo "DONE $(utc) $MODE $NN rc_so_far=$LEG_RC"
  done
done
batch_end screen
echo "FOREGROUND_END $(utc) screen"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
exit $LEG_RC
