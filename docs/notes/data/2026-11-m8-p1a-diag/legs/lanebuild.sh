#!/usr/bin/env bash
# usage: box-lane -- setsid -w bash lanebuild.sh   (hven lib, sequential MKL, install to .scratch/prefix; then lane binary)
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/p1a-diag
HB=/home/ghecht/Projects/hven-bench-p1a
B=$WT/.scratch/lanelib; P=$WT/.scratch/prefix
export CCACHE_DISABLE=1 TMPDIR=$WT/.scratch/tmp GIT_CEILING_DIRECTORIES=$WT
J=${BOX_LANE_JOBS:-4}
echo "START $(date -Is)"
if [ ! -f "$B/build.ninja" ]; then
  cmake -S "$WT" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
    -DHVEN_BUILD_TESTS=OFF -DHVEN_BUILD_BENCH=OFF -DHVEN_LINK_TIME_OPT=OFF -DHVEN_MKL_THREADING=sequential \
    -DCMAKE_INSTALL_PREFIX="$P" > "$B.configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -20 "$B.configure.log"; exit 1; }
fi
ninja -C "$B" -j$J > "$B.build.log" 2>&1 || { echo LIB_BUILD_FAIL; grep -m5 error: "$B.build.log"; exit 1; }
cmake --install "$B" > "$B.install.log" 2>&1 || { echo INSTALL_FAIL; exit 1; }
echo "HVEN_PIN_SHA=$(git -C $WT rev-parse HEAD) (dirty=$(git -C $WT status --porcelain -uno | wc -l))" > "$P/HVEN_PIN.txt"
LB=$HB/build-p1a
if [ ! -f "$LB/build.ninja" ]; then
  cmake -S "$HB" -B "$LB" -G Ninja -Uhven_DIR -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_PREFIX_PATH="$HB/.hven/install-p1adiag00000" > "$LB.configure.log" 2>&1 || { echo LANE_CONFIGURE_FAIL; tail -20 "$LB.configure.log"; exit 1; }
fi
ninja -C "$LB" -j$J hven_bench_cell > "$LB.build.log" 2>&1 || { echo LANE_BUILD_FAIL; grep -m5 error: "$LB.build.log"; exit 1; }
ls -la "$LB/lanes/hven/hven_bench_cell"
echo "rc=0 END $(date -Is)"
