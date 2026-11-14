#!/usr/bin/env bash
# NaN probe: lpNorm<Infinity> vs maxCoeff<PropagateNaN>, Release (the library's flags) and Debug.
set -o pipefail
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
P=${PROBE_DIR:-/home/ghecht/Projects/hven/.claude/worktrees/nan/build-nan/probe}   # the sources are this directory's copies
E=/home/ghecht/Projects/hven/.claude/worktrees/nan/dep/eigen
REL="-DEIGEN_DONT_PARALLELIZE -DEIGEN_INITIALIZE_MATRICES_BY_ZERO -DEIGEN_MAX_ALIGN_BYTES=32 -m64 -O3 -DNDEBUG -std=c++20 -pthread -mllvm -inline-threshold=225 -fomit-frame-pointer -fno-stack-protector -fno-asynchronous-unwind-tables -falign-loops=32 -march=native -mtune=native -ffast-math -fno-finite-math-only"
DBG="$1"
for arm in rel dbg; do
  if [ $arm = rel ]; then F="$REL"; else F="$DBG"; fi
  echo "== $arm flags: $F"
  /usr/bin/clang++ $F -isystem $E -c $P/norms.cpp -o $P/norms-$arm.o && /usr/bin/clang++ $F -isystem $E -c $P/main.cpp -o $P/main-$arm.o && /usr/bin/clang++ $F $P/norms-$arm.o $P/main-$arm.o -o $P/probe-$arm || exit 1
  $P/probe-$arm
done
echo "FOREGROUND_END $(date -Is)"
