#!/usr/bin/env bash
# The SQP-side wall leg: the arm that CAN observe the seam changes, since the
# interior-point image links no seam symbol at all. Three standing cells,
# base/head alternated per rep so machine drift lands on both sides.
set -u
D=/home/ghecht/Projects/hven/.scratch/task-7/stage2
AB=$D/ab
REPS=${1:-7}
{ echo "=== sqp leg start $(date -Is)"; echo "=== loadavg-before: $(cat /proc/loadavg)"; } > "$D/sqpleg.meta"
flock -w 14400 /tmp/box-build.lock bash -c "
  echo '=== lock acquired '\$(date -Is) >> '$D/sqpleg.meta'
  for i in \$(seq 1 60); do
    L=\$(cut -d' ' -f1 /proc/loadavg)
    if awk \"BEGIN{exit !(\$L < 0.6)}\"; then break; fi
    sleep 15
  done
  echo '=== loadavg-at-leg-start: '\$(cat /proc/loadavg) >> '$D/sqpleg.meta'
  ps -eo comm,pcpu --sort=-pcpu | head -5 >> '$D/sqpleg.meta'
  : > '$AB/sqp_leg.txt'
  for r in \$(seq 1 $REPS); do
    for side in base head; do
      B='$D/wt-'\$side'/build/bench/hven_sqp_bench'
      for cell in 'F3 1000 cold' 'F3 1000 warm' 'F7 200 cold'; do
        set -- \$cell
        T0=\$(date +%s%N)
        env MKL_NUM_THREADS=1 taskset -c 2 \$B --family \$1 --n \$2 --arm \$3 \
            --sweep 5 --csv '$AB/sqp_scratch.csv' > /dev/null 2>&1
        T1=\$(date +%s%N)
        S=\$(awk \"BEGIN{printf \\\"%.4f\\\", (\$T1-\$T0)/1e9}\")
        echo \"rep\$r \$side \$1n\$2\$3 \$S\" >> '$AB/sqp_leg.txt'
      done
    done
  done
  echo \"leg_rc=\$?\" >> '$D/sqpleg.meta'
  # The images that produced the numbers, identified and dumped in the same
  # breath as the timings: an unfiltered nm of each side, so a later reader can
  # see instantiations the seam's own name does not appear in.
  for side in base head; do
    md5sum '$D/wt-'\$side'/build/bench/hven_sqp_bench' >> '$D/sqpleg.meta'
    nm -C --print-size --size-sort '$D/wt-'\$side'/build/bench/hven_sqp_bench' \
      > '$AB/nm_full_'\$side'.txt' 2>/dev/null
  done
  echo '=== loadavg-at-leg-end: '\$(cat /proc/loadavg) >> '$D/sqpleg.meta'
"
echo "=== outer_rc=$? $(date -Is)" >> "$D/sqpleg.meta"
echo "=== DONE" >> "$D/sqpleg.meta"
