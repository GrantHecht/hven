#!/bin/bash
# W5 T8.9r-attrib5 gate (round 2): the PUBLIC interior leg at the LEVER'S HEAD,
# run under THE COMMITTED BASELINE'S OWN --cells spec (read out of that file's
# provenance header), twice, and compared against it on every non-wall column.
cd / && cd /home/ghecht/Projects/hven || exit 70
R=/home/ghecht/Projects/hven/.scratch/w5t89ra5
REPO=/home/ghecht/Projects/hven
. $R/common.sh
LOG=$R/logs/G11-interior-captures.log
CMP=$REPO/docs/notes/data/2026-09-m6-w4-acceptance/probes/compare_replay.py
BASE=$REPO/bench/baselines/2026-09-t8-ipm-leg/interior_baseline.csv
OUT=$R/captures
mkdir -p $OUT
CELLS=$(grep -a -m1 '^# invocation:' $BASE | sed -E 's/.*--cells ([^ ]+).*/\1/')
{
echo "GATE 11  the public --engine interior leg at the lever's head, two captures"
echo "PGREP_SEPARATE $(utc) -- taken OUTSIDE the lock; process count:"
wc -l < $R/logs/G11-pgrep-separate.txt
grep -c 'cmake\|ninja\|ctest\|hven_' $R/logs/G11-pgrep-separate.txt
echo "--- inside lock, build/solve processes only ---"
pgrep -af 'cmake|ninja|ctest|hven_' || echo PGREP_EMPTY_BUILD
echo "FOREGROUND_START $(utc) gate=11"
echo "binary: $(sha256sum $REPO/build/bench/hven_sqp_corpus)"
echo "cells (from the baseline's own provenance): $CELLS"
for n in 5 6; do
  echo "=== capture $n $(utc) ==="
  env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 $REPO/build/bench/hven_sqp_corpus \
      --engine interior --cells "$CELLS" --csv $OUT/capture$n.csv > $OUT/capture$n.stdout 2>&1
  fold $?
  tail -1 $OUT/capture$n.stdout
done
echo "=== compare $(utc) ==="
for n in 5 6; do
  python3 $CMP $BASE $OUT/capture$n.csv "committed baseline vs capture $n" | cut -c1-200
  fold ${PIPESTATUS[0]}
done
python3 $CMP $OUT/capture5.csv $OUT/capture6.csv "capture 5 vs capture 6" | cut -c1-200
fold ${PIPESTATUS[0]}
echo "=== byte claim outside wall_s $(utc) ==="
grep -v '^#' $BASE | cut -d, -f1-30 > $OUT/base_nowall.csv
for n in 5 6; do
  grep -v '^#' $OUT/capture$n.csv | cut -d, -f1-30 > $OUT/cap${n}_nowall.csv
  if cmp -s $OUT/base_nowall.csv $OUT/cap${n}_nowall.csv; then
    echo "capture $n: BYTE-IDENTICAL to the committed baseline outside wall_s ($(grep -vc '^#' $OUT/capture$n.csv) lines = header + 43 rows)"
  else
    echo "capture $n: BYTE DIFFERENCE outside wall_s"; cmp $OUT/base_nowall.csv $OUT/cap${n}_nowall.csv; fold 1
  fi
done
echo "FOREGROUND_END $(utc) gate=11"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
rc=$LEG_RC; echo "WRAPPER_EXIT=$rc"; exit $rc
