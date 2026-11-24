#!/usr/bin/env bash
# usage: box-lane -- setsid -w bash build.sh <Release|Debug> <dir> [targets...]
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/p1a-diag
TYPE=${1:?}; DIR=$WT/.scratch/${2:?}; shift 2
export CCACHE_DISABLE=1 TMPDIR=$WT/.scratch/tmp GIT_CEILING_DIRECTORIES=$WT
mkdir -p "$TMPDIR"
J=${BOX_LANE_JOBS:-4}
echo "START $(date -Is) lane=${BOX_LANE:-?} j=$J"
if [ ! -f "$DIR/build.ninja" ]; then
  cmake -S "$WT" -B "$DIR" -G Ninja -DCMAKE_BUILD_TYPE="$TYPE" -DHVEN_FP_MODE=SAFER_FAST \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE \
    -DHVEN_BUILD_BENCH=ON -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/home/ghecht/Projects/hven/build/_deps/googletest-src > "$DIR.configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -30 "$DIR.configure.log"; exit 1; }
fi
ninja -C "$DIR" -j$J -k 0 "$@" > "$DIR.build.log" 2>&1
rc=$?
tail -3 "$DIR.build.log"; grep -m8 'error:' "$DIR.build.log"
echo "rc=$rc END $(date -Is)"
exit $rc
