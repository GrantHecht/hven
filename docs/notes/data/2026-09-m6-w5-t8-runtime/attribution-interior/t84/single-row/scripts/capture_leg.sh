#!/bin/bash
# W5 T8.9r-attrib5 gate: the PUBLIC interior leg, unchanged. Two captures at the
# lever's head, each compared against the COMMITTED 43-row baseline on every
# non-wall column, and against each other.
cd / && cd /home/ghecht/Projects/hven || exit 70
R=/home/ghecht/Projects/hven/.scratch/w5t89ra5
REPO=/home/ghecht/Projects/hven
. $R/common.sh
LOG=$R/logs/G05-interior-captures.log
CMP=$REPO/docs/notes/data/2026-09-m6-w4-acceptance/probes/compare_replay.py
BASE=$REPO/bench/baselines/2026-09-t8-ipm-leg/interior_baseline.csv
OUT=$R/captures
mkdir -p $OUT
{
echo "GATE 05  the public --engine interior leg, two captures"
echo "PGREP_SEPARATE $(utc) -- taken OUTSIDE the lock, verbatim:"
cat $R/logs/G05-pgrep-separate.txt
echo "--- inside lock ---"
box_pgrep
echo "FOREGROUND_START $(utc) gate=05"
echo "binary: $(sha256sum $REPO/build/bench/hven_sqp_corpus)"
for n in 1 2; do
  echo "=== capture $n $(utc) ==="
  env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 $REPO/build/bench/hven_sqp_corpus \
      --engine interior --cells all --csv $OUT/capture$n.csv > $OUT/capture$n.stdout 2>&1
  fold $?
  echo "rows: $(grep -vc '^#' $OUT/capture$n.csv) (header + data)"
  tail -2 $OUT/capture$n.stdout
done
echo "=== compare $(utc) ==="
run python3 $CMP $BASE $OUT/capture1.csv "committed baseline vs capture 1"
run python3 $CMP $BASE $OUT/capture2.csv "committed baseline vs capture 2"
run python3 $CMP $OUT/capture1.csv $OUT/capture2.csv "capture 1 vs capture 2"
echo "=== byte claim outside wall_s $(utc) ==="
for n in 1 2; do
  grep -v '^#' $BASE | cut -d, -f1-30 > $OUT/base_nowall.csv
  grep -v '^#' $OUT/capture$n.csv | cut -d, -f1-30 > $OUT/cap${n}_nowall.csv
  if cmp -s $OUT/base_nowall.csv $OUT/cap${n}_nowall.csv; then
    echo "capture $n: BYTE-IDENTICAL to the committed baseline outside wall_s ($(wc -l < $OUT/base_nowall.csv) lines, header + 43 rows)"
  else
    echo "capture $n: BYTE DIFFERENCE outside wall_s"; cmp $OUT/base_nowall.csv $OUT/cap${n}_nowall.csv; fold 1
  fi
done
echo "FOREGROUND_END $(utc) gate=05"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
rc=$LEG_RC; echo "WRAPPER_EXIT=$rc"; exit $rc
