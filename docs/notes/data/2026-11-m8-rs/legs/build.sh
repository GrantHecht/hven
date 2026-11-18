#!/usr/bin/env bash
# usage: build.sh <Release|Debug> <build-dir-name> [clean]   Run as: flock /tmp/box-build.lock setsid -w bash build.sh ...
# Clean configure (CCACHE_DISABLE=1) of the linux-clang preset flags + bench + examples, then the full build.
set -o pipefail
source "$(dirname "$0")/env.sh"
box_check
TYPE=${1:?Release|Debug}; DIR=$WT/${2:?dir}
[ "${3:-}" = clean ] && rm -rf "$DIR"
echo "FOREGROUND_START $(date -Is)"
if [ ! -f "$DIR/build.ninja" ]; then
  GIT_DIR=$(git -C "$WT" rev-parse --absolute-git-dir) GIT_WORK_TREE=$WT \
  cmake -S "$WT" -B "$DIR" -G Ninja -DCMAKE_BUILD_TYPE="$TYPE" -DHVEN_FP_MODE=SAFER_FAST \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE \
    -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_EXAMPLES=ON -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/home/ghecht/Projects/hven/build/_deps/googletest-src > "$DIR.configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -30 "$DIR.configure.log"; exit 1; }
fi
ninja -C "$DIR" -j16 -k 0 > "$DIR.build.log" 2>&1
rc=$?
tail -3 "$DIR.build.log"; echo "build rc=$rc"; echo "FOREGROUND_END $(date -Is)"
exit $rc
