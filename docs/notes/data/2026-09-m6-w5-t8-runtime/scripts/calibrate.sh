#!/bin/bash
# W5 T8.9r L3: the leg-2 calibration (median_se_pct vs N on the A arm alone)
# AND the leg-1 / interior timing survey that sizes every later leg.
R=/home/ghecht/Projects/hven/.scratch/w5t89r
. $R/common.sh
LOG=$R/logs/L3-calibration.log
A=$R/arm-base/build/bench/hven_sqp_corpus          # the A arm, alone (11.3)
Hh=$R/arm-head/build/bench/hven_sqp_corpus
CELLS="$(sed -n '3p' /home/ghecht/Projects/hven/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)"
mkdir -p $R/calib
{
echo "PGREP_SEPARATE $(utc)"
cat $R/logs/L3-pgrep-separate.txt
box_pgrep
echo "FOREGROUND_START $(utc)"
echo
echo "### PART 1 -- leg 2 calibration: worst per-cell median_se_pct vs N, A arm (102f729) alone,"
echo "### solo, taskset -c 2, MKL_NUM_THREADS=OMP_NUM_THREADS=1, warmup 1, trace off."
for eng in ipm walk ssn; do
  for N in 125 250 500 1000 2000; do
    f=$R/calib/hs-$eng-N$N.csv
    s=$(date +%s.%N)
    env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
        $A --hs --engine $eng --repeat $N --hs-warmup 1 --csv $f > $R/calib/hs-$eng-N$N.out 2>&1
    fold $?
    e=$(date +%s.%N)
    worst=$(grep "worst per-cell median SE" $R/calib/hs-$eng-N$N.out | head -1)
    corpus=$(grep "corpus (sum of per-cell medians)" $R/calib/hs-$eng-N$N.out | head -1)
    echo "CAL $eng N=$N elapsed=$(printf '%.2f' $(echo "$e - $s" | bc)) | $worst | $corpus"
  done
done
echo
echo "### PART 2 -- leg 1 timing survey: ONE full U0 27-cell run per mode, each arm."
for eng in walk ssn ipm; do
  for arm in base head; do
    bin=$R/arm-$arm/build/bench/hven_sqp_corpus
    s=$(date +%s.%N)
    env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
        $bin --engine $eng --cells "$CELLS" --csv $R/calib/survey-$eng-$arm.csv > $R/calib/survey-$eng-$arm.out 2>&1
    fold $?
    e=$(date +%s.%N)
    sum=$(awk -F, 'NR>1 && $0 !~ /^#/ {s+=$14} END {printf "%.4f", s}' <(grep -v '^#' $R/calib/survey-$eng-$arm.csv))
    echo "SURVEY leg1 $eng $arm wallclock=$(printf '%.2f' $(echo "$e - $s" | bc)) corpus_wall_s=$sum rows=$(($(grep -vc '^#' $R/calib/survey-$eng-$arm.csv)-1))"
  done
done
echo
echo "### PART 3 -- interior timing survey: ONE full run per arm (head 43 rows, interior-base 41)."
for arm in interior-base head; do
  bin=$R/arm-$arm/build/bench/hven_sqp_corpus
  s=$(date +%s.%N)
  env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
      $bin --engine interior --cells "$CELLS" --csv $R/calib/survey-interior-$arm.csv > $R/calib/survey-interior-$arm.out 2>&1
  fold $?
  e=$(date +%s.%N)
  echo "SURVEY interior $arm wallclock=$(printf '%.2f' $(echo "$e - $s" | bc)) rows=$(($(grep -vc '^#' $R/calib/survey-interior-$arm.csv)-1))"
done
echo "FOREGROUND_END $(utc)"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
exit $LEG_RC
