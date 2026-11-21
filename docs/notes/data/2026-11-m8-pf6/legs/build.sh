#!/usr/bin/env bash
# usage: build.sh <worktree> <Release|Debug> <build-dir-name> [clean] [ninja targets...]
#   Run as: flock /tmp/box-build.lock setsid -w bash build.sh ...
# Configure (CCACHE_DISABLE=1, linux-clang flags, bench + examples ON) then ninja. The worktree is an argument: the same script builds arm A and arm B.
set -o pipefail
WT=${1:?worktree}; TYPE=${2:?Release|Debug}; DIR=$WT/${3:?dir}; shift 3
CLEAN=0; [ "${1:-}" = clean ] && { CLEAN=1; shift; }
export TMPDIR=$WT/.scratch/tmp MKL_NUM_THREADS=1 CCACHE_DISABLE=1 GIT_CEILING_DIRECTORIES=$WT
mkdir -p "$TMPDIR"
echo PGREP_EMPTY; echo "FOREGROUND_START $(date -Is)"
[ $CLEAN = 1 ] && rm -rf "$DIR"
if [ ! -f "$DIR/build.ninja" ]; then
  GIT_DIR=$(git -C "$WT" rev-parse --absolute-git-dir) GIT_WORK_TREE=$WT \
  cmake -S "$WT" -B "$DIR" -G Ninja -DCMAKE_BUILD_TYPE="$TYPE" -DHVEN_FP_MODE=SAFER_FAST \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE \
    ${HVEN_CMAKE_EXTRA:-} -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_EXAMPLES=ON -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/home/ghecht/Projects/hven/build/_deps/googletest-src > "$DIR.configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -30 "$DIR.configure.log"; exit 1; }
fi
if [ "${CONFIGURE_ONLY:-0}" = 1 ]; then echo CONFIGURE_ONLY_DONE; exit 0; fi
ninja -C "$DIR" -j${BOX_LANE_JOBS:-4} -k 0 "$@" > "$DIR.build.log" 2>&1; rc=$?
tail -3 "$DIR.build.log"; echo "build rc=$rc"; echo "FOREGROUND_END $(date -Is)"; exit $rc
