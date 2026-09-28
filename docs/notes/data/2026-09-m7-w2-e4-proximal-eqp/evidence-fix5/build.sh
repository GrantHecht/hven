#!/usr/bin/env bash
# usage: build.sh  -- configure(if needed)+build+install the scratch hven, then
# build the bench copy's hven_bench_cell against it.  Run INSIDE leg.sh.
set -o pipefail
S=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9/.scratch/e4-fix5
PREFIX=$S/bench/.hven/install-ff02789b46a7
mkdir -p $S/logs
if [ ! -d $S/build-rel ]; then
  cmake -S $S/src -B $S/build-rel -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
    -DHVEN_BUILD_TESTS=OFF -DHVEN_BUILD_BENCH=OFF -DHVEN_LINK_TIME_OPT=OFF \
    -DCMAKE_INSTALL_PREFIX=$PREFIX > $S/logs/cfg-hven.log 2>&1 || { tail -30 $S/logs/cfg-hven.log; exit 81; }
fi
cmake --build $S/build-rel --parallel 16 > $S/logs/build-hven.log 2>&1 || { tail -40 $S/logs/build-hven.log; exit 82; }
cmake --install $S/build-rel > $S/logs/install-hven.log 2>&1 || { tail -20 $S/logs/install-hven.log; exit 83; }
# the stamp the bench copy's runner/CMake expects
grep -m1 'EIGEN_MAX_ALIGN_BYTES resolved to' $S/logs/cfg-hven.log > /dev/null 2>&1 || true
{ echo "HVEN_PIN_SHA ff02789b46a7a55017c2bf96dbf31ff47c5b0b4e"
  echo "HVEN_PIN_BRANCH m7"
  echo "HVEN_PIN_DATE 2026-09-27"
  echo "HVEN_BUILT_UTC $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "HVEN_CXX_COMPILER $(clang++ --version | head -1)"
  echo "HVEN_DIAG_TREE $S/src  (E4 FIX5 diagnostic scratch; sources = 84fb520e == ff02789b sources, PLUS any env-gated diag patch)"
} > $PREFIX/HVEN_PIN.txt
echo HVEN_INSTALL_OK
if [ ! -d $S/bench-build ]; then
  cmake -S $S/bench -B $S/bench-build -G Ninja -Uhven_DIR \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_PREFIX_PATH=$PREFIX > $S/logs/cfg-bench.log 2>&1 || { tail -40 $S/logs/cfg-bench.log; exit 84; }
fi
cmake --build $S/bench-build --target hven_bench_cell > $S/logs/build-bench.log 2>&1 || { tail -40 $S/logs/build-bench.log; exit 85; }
echo BENCH_BUILD_OK
ls -la $S/bench-build/lanes/hven/hven_bench_cell
