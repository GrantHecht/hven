#!/usr/bin/env bash
# M8 F1: the before/after. hs_probe (one IPM solve per fresh process) on hs006 hs010 hs035 hs071, arm A vs arm B,
# 5 rounds, arm order alternated per round (A B, B A, ...), core 2, MKL_NUM_THREADS=1, serial, alone, under
# perf stat -e instructions:u. Each process starts phase-aligned (date +%N < 5e6, as the runtime gate's leg).
# usage: hs_measure.sh <out-dir>     Run as: flock /tmp/box-build.lock bash hs_measure.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/f1
OUT=${1:?out dir}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
mkdir -p "$OUT/raw"
align() { while :; do ns=$(date +%N); [ $((10#$ns)) -lt 5000000 ] && return 0; done; }
sha256sum "$WT/build-f1/hs-A/hs_probe" "$WT/build-f1/hs-B/hs_probe"
for r in 1 2 3 4 5; do
  order="A B"; [ $((r % 2)) = 0 ] && order="B A"
  for p in hs006 hs010 hs035 hs071; do
    for arm in $order; do
      b=$OUT/raw/$p-r$r-$arm
      align
      taskset -c 2 perf stat -x, -e instructions:u -o "$b.stat" -- "$WT/build-f1/hs-$arm/hs_probe" $p > "$b.out" 2>&1
      echo "RUN $p r$r $arm rc=$?"
    done
  done
done
python3 "$WT/docs/notes/data/2026-11-m8-f1-init/legs/hs_analyze.py" "$OUT" | tee "$OUT/analysis.txt"
rc=${PIPESTATUS[0]}
echo "rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
