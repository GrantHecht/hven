#!/bin/bash
# $1 = source root, $2 = build dir with libhven.a, $3 = output binary
set -e
SRC="$1"; BLD="$2"; OUT="$3"
/usr/bin/clang++ -DEIGEN_DONT_PARALLELIZE -DEIGEN_INITIALIZE_MATRICES_BY_ZERO \
  -DEIGEN_MAX_ALIGN_BYTES=32 -DFMT_HEADER_ONLY -DFMT_USE_LOCALE=0 -DHVEN_DEFAULT_QP_THREADS=8 \
  -I"$SRC/include" -I/opt/intel/oneapi/mkl/latest/include \
  -isystem "$SRC/dep/eigen" -isystem "$SRC/dep/fmt/include" \
  -DMKL_LP64 -m64 -O3 -DNDEBUG -std=c++20 -pthread -march=native -mtune=native \
  -ffast-math -fno-finite-math-only -fopenmp=libiomp5 -L/opt/intel/oneapi/compiler/latest/lib \
  /home/ghecht/Projects/hven/.scratch/task-3/ab/ab_probe.cpp -o "$OUT" \
  -Wl,-rpath,/opt/intel/oneapi/mkl/latest/lib:/opt/intel/oneapi/compiler/latest/lib \
  "$BLD/libhven.a" -Wl,--start-group /opt/intel/oneapi/mkl/latest/lib/libmkl_intel_lp64.a \
  /opt/intel/oneapi/mkl/latest/lib/libmkl_intel_thread.a \
  /opt/intel/oneapi/mkl/latest/lib/libmkl_core.a \
  /opt/intel/oneapi/compiler/latest/lib/libiomp5.so -Wl,--end-group -ldl -lm
