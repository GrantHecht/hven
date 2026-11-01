#!/bin/bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a026c6bee01193dcb
B=$W/.scratch/a1/build-rel
P=$W/.scratch/a1/probe
echo FOREGROUND_START $(date -Is)
flock /tmp/box-build.lock bash -c "
set -o pipefail
cd $B &&
/usr/bin/clang++ -DEIGEN_DONT_PARALLELIZE -DEIGEN_INITIALIZE_MATRICES_BY_ZERO -DEIGEN_MAX_ALIGN_BYTES=32 -DFMT_HEADER_ONLY -DFMT_USE_LOCALE=0 -DHVEN_DEFAULT_QP_THREADS=8 -I$W/bench -I$W/tests/sqp -I$W/include -I/opt/intel/oneapi/mkl/latest/include -isystem $W/dep/eigen -isystem $W/dep/fmt/include -DMKL_LP64 -m64 -O3 -DNDEBUG -std=c++20 -fPIE -pthread -mllvm -inline-threshold=225 -fomit-frame-pointer -fno-stack-protector -fno-asynchronous-unwind-tables -falign-loops=32 -march=native -mtune=native -ffast-math -fno-finite-math-only -fopenmp=libiomp5 -c $P/a1_probe.cpp -o $P/a1_probe3.o &&
/usr/bin/clang++ -DMKL_LP64 -m64 -O3 -DNDEBUG $P/a1_probe3.o -o $P/a1_probe3 -Wl,-rpath,/opt/intel/oneapi/mkl/latest/lib:/opt/intel/oneapi/compiler/latest/lib libhven.a -Wl,--start-group /opt/intel/oneapi/mkl/latest/lib/libmkl_intel_lp64.a /opt/intel/oneapi/mkl/latest/lib/libmkl_intel_thread.a /opt/intel/oneapi/mkl/latest/lib/libmkl_core.a /opt/intel/oneapi/compiler/latest/lib/libiomp5.so -Wl,--end-group -ldl -lm
"
rc=$?
echo FOREGROUND_END rc=$rc $(date -Is)
