#!/usr/bin/env bash
# usage: lane_install.sh <intel|sequential> <sha12>   Run as: flock /tmp/box-build.lock setsid -w bash lane_install.sh ...
# Builds pf6 HEAD (this worktree) with the pin build's own flags (scripts/build_hven_pin.sh: Release, clang++, tests/bench OFF, LTO OFF)
# plus -DHVEN_MKL_THREADING=<layer>, installs into the private harness worktree's .hven/install-<sha12>, then configures and builds hven_bench_cell there.
set -euo pipefail
LAYER=${1:?}; SHA12=${2:?}
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf6
HB=/home/ghecht/Projects/hven-bench-pf6-${LAYER}
export TMPDIR=$HB/.scratch MKL_NUM_THREADS=1 CCACHE_DISABLE=1 GIT_CEILING_DIRECTORIES=$WT
mkdir -p "$TMPDIR"
echo "FOREGROUND_START lane_install $LAYER $(date -Is)"
PREFIX=$HB/.hven/install-$SHA12; BD=$HB/.hven/build-$SHA12
rm -rf "$BD" "$PREFIX"
cmake -S "$WT" -B "$BD" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -DHVEN_BUILD_TESTS=OFF -DHVEN_BUILD_BENCH=OFF -DHVEN_LINK_TIME_OPT=OFF \
  -DHVEN_MKL_THREADING=$LAYER -DCMAKE_INSTALL_PREFIX="$PREFIX" > "$HB/.scratch/hven-configure.log" 2>&1
cmake --build "$BD" --parallel ${BOX_LANE_JOBS:-4} > "$HB/.scratch/hven-build.log" 2>&1
cmake --install "$BD" > "$HB/.scratch/hven-install.log" 2>&1
printf 'HVEN_PIN_SHA %s\nHVEN_MKL_THREADING %s\n' "$(git -C $WT rev-parse HEAD)" "$LAYER" > "$PREFIX/HVEN_PIN.txt"
cd "$HB"
bash configure.sh > "$HB/.scratch/bench-configure.log" 2>&1
cmake --build build --parallel ${BOX_LANE_JOBS:-4} > "$HB/.scratch/bench-build.log" 2>&1
sha256sum build/lanes/hven/hven_bench_cell
echo "FOREGROUND_END lane_install $LAYER $(date -Is)"
