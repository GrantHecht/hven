#!/usr/bin/env bash
# M8 F1 diagnosis: build mkl_probe (MKL static LP64 + intel_thread + libiomp5, hven's link shape).
# Run as: flock /tmp/box-build.lock bash build_probe.sh <out-dir>
set -o pipefail
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo PGREP_BUSY; exit 3; fi
echo PGREP_EMPTY; echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1
HERE=$(dirname "$(readlink -f "$0")"); OUT=${1:?out}; mkdir -p "$OUT"
M=/opt/intel/oneapi/mkl/latest
/usr/bin/clang -O2 -DMKL_LP64 -m64 -I$M/include "$HERE/mkl_probe.c" -o "$OUT/mkl_probe" \
  -Wl,--start-group $M/lib/libmkl_intel_lp64.a $M/lib/libmkl_intel_thread.a $M/lib/libmkl_core.a -Wl,--end-group \
  -L/opt/intel/oneapi/compiler/latest/lib -Wl,-rpath,/opt/intel/oneapi/compiler/latest/lib -liomp5 -lpthread -lm -ldl
rc=$?; echo "rc=$rc"; echo "FOREGROUND_END $(date -Is)"; exit $rc
