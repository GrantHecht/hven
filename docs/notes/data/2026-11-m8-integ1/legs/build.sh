#!/usr/bin/env bash
# usage: build.sh <source dir> <Release|Debug> <build dir>   Run under `box-lane --`.  Same flag regime as the I17 gate (bench + examples ON, SAFER_FAST, clang++).
# usage note: SRC is the trs1 worktree.
set -o pipefail
SRC=${1:?src}; TYPE=${2:?type}; DIR=${3:?dir}
export TMPDIR=/home/ghecht/Projects/hven/.claude/worktrees/integ1/.scratch/tmp CCACHE_DISABLE=1 MKL_NUM_THREADS=1
mkdir -p "$TMPDIR"
echo "FOREGROUND_START $(date -Is)"
cmake -S "$SRC" -B "$DIR" -G Ninja -DCMAKE_BUILD_TYPE="$TYPE" -DHVEN_FP_MODE=SAFER_FAST -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_EXAMPLES=ON -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/home/ghecht/Projects/hven/build/_deps/googletest-src > "$DIR.configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -20 "$DIR.configure.log"; exit 1; }
ninja -C "$DIR" -j${BOX_LANE_JOBS:-4} -k 0 > "$DIR.build.log" 2>&1; rc=$?
tail -2 "$DIR.build.log"; echo "build rc=$rc"; echo "FOREGROUND_END $(date -Is)"; exit $rc
