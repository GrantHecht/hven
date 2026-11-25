#!/usr/bin/env bash
# M8 F1 diagnosis: mkl_probe <mode> nolate (no trailing dsecnd) under perf stat instructions:u, 5x, core 2.
# Run as: flock /tmp/box-build.lock bash run_probe2.sh <probe-bin> <out-file>
set -o pipefail
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo PGREP_BUSY; exit 3; fi
echo PGREP_EMPTY; echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
BIN=${1:?bin}; OUT=${2:?out}; : > "$OUT"
for r in 1 2 3 4 5; do for m in none maxthreads version; do
  echo "== round $r mode $m nolate" >> "$OUT"
  taskset -c 2 perf stat -x, -e instructions:u -o /dev/stdout -- "$BIN" $m nolate 2>&1 | grep -v '^#' | grep -v '^$' >> "$OUT"
done; done
echo "FOREGROUND_END $(date -Is)"
