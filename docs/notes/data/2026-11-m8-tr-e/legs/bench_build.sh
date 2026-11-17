#!/usr/bin/env bash
# usage: bench_build.sh <old|new>      Run as: flock /tmp/box-build.lock bash bench_build.sh <arm>
# Builds the hven install prefix at the arm's declared pin (scripts/build_hven_pin.sh, from the private clone's own
# cmake/hven_pin.cmake) and the arm's hven lane binary, in a PRIVATE clone of hven-bench under this worktree's .scratch
# (old = 7d974e8, pin bdd797e3; new = branch tr-e, pin 24b2a7e4). The main hven-bench checkout is not built in.
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/tr-e
ARM=${1:?old|new}
R=$WT/.scratch/bench-$ARM
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 TMPDIR=$WT/.scratch/tmp GIT_CEILING_DIRECTORIES=$WT/.scratch
mkdir -p "$TMPDIR"
cd "$R" || exit 1
git rev-parse HEAD; grep '^set(HVEN_PIN_SHA' cmake/hven_pin.cmake
setsid -w bash scripts/build_hven_pin.sh || { echo "PIN_BUILD_FAIL"; exit 1; }
GIT_DIR=$R/.git GIT_WORK_TREE=$R setsid -w bash ./configure.sh -DHVEN_BENCH_CUTEST_ROOT=/home/ghecht/Software/cutest \
  > "$WT/.scratch/legs/configure-$ARM.log" 2>&1 || { echo CONFIGURE_FAIL; tail -30 "$WT/.scratch/legs/configure-$ARM.log"; exit 1; }
setsid -w ninja -C "$R/build" -j16 hven_bench_cell
rc=$?
echo "rc=$rc"
sha256sum "$R/build/lanes/hven/hven_bench_cell"
echo "FOREGROUND_END $(date -Is)"
exit $rc
