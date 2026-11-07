#!/usr/bin/env bash
# B2-runtime-r2 INSTRUMENT PREFLIGHT (not a gate leg; no verdict): does fixed-period
# sampling at the settler-specified period capture every instruction on this box?
# One interior-leg process of the B2 code head per period, perf stat nested inside perf
# record (the r2 arrangement), samples*period compared with perf stat's count, and the
# kernel's THROTTLE events counted. Run as: flock /tmp/box-build.lock bash preflight_throttle.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-aba3deeb97313f444
EV=$WT/docs/notes/data/2026-11-m8-ap2-b2/runtime-r2
SCRATCH=$WT/build-b2/r2-perf
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$SCRATCH"
BIN=$WT/build-b2/rel/bench/hven_sqp_corpus
sha256sum "$BIN"
echo "perf_event_max_sample_rate=$(cat /proc/sys/kernel/perf_event_max_sample_rate) perf_cpu_time_max_percent=$(cat /proc/sys/kernel/perf_cpu_time_max_percent) perf_event_paranoid=$(cat /proc/sys/kernel/perf_event_paranoid)"
CELLS=$(sed -n 3p docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)
for PERIOD in 20000 200000; do
  env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
      perf record -q -c $PERIOD -e instructions:u -o "$SCRATCH/pre-$PERIOD.data" -- \
      perf stat -x, -e instructions:u -o "$SCRATCH/pre-$PERIOD.stat" -- \
      "$BIN" --engine interior --cells "$CELLS" --csv "$SCRATCH/pre-$PERIOD.csv" > /dev/null 2>&1
  rc=$?
  RAW=$(grep instructions "$SCRATCH/pre-$PERIOD.stat" | cut -d, -f1)
  SAMPLES=$(perf report -i "$SCRATCH/pre-$PERIOD.data" --comm hven_sqp_corpus --sort symbol --stdio \
      --no-children -F sample,sym 2> /dev/null | grep -v '^#' | grep -v '^$' | awk '{s+=$1} END{print s}')
  THR=$(perf report -i "$SCRATCH/pre-$PERIOD.data" --stats 2> /dev/null | grep -E '^ +THROTTLE events' | awk '{print $3}')
  python3 -c "print('period=%d rc=%d raw_instructions=%s samples=%s samples*period/raw=%.6f throttle_events=%s' % ($PERIOD, $rc, '$RAW', '$SAMPLES', $SAMPLES*$PERIOD/float('$RAW'), '${THR:-0}'))"
  rm -f "$SCRATCH/pre-$PERIOD.data"
done
echo "FOREGROUND_END $(date -Is)"
