#!/usr/bin/env bash
# RF-1 build recipe (pf2d legs/build.sh re-pointed). usage: build.sh <source-dir> <Release|Debug> <dir-name> [ninja targets...]
# Run under: ~/.local/bin/box-lane -- bash build.sh ...   (-j$BOX_LANE_JOBS; TMPDIR in the worktree)
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/rf1
SRC=${1:?source}; TYPE=${2:?Release|Debug}; DIR=$WT/.scratch/build-rf1/${3:?dir}; shift 3
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1 TMPDIR=$WT/.scratch/tmp
mkdir -p "$WT/.scratch/build-rf1" "$TMPDIR"
if [ ! -f "$DIR/build.ninja" ]; then
  cmake -S "$SRC" -B "$DIR" -G Ninja -DCMAKE_BUILD_TYPE="$TYPE" -DHVEN_FP_MODE=SAFER_FAST \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE \
    -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_EXAMPLES=ON > "$DIR.configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -30 "$DIR.configure.log"; exit 1; }
fi
ninja -C "$DIR" -j${BOX_LANE_JOBS:-4} -k 0 "$@" > "$DIR.build.log" 2>&1
rc=$?; tail -3 "$DIR.build.log"; echo "rc=$rc"; exit $rc
