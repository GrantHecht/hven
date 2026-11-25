#!/usr/bin/env bash
# usage: build.sh <Release|Debug> <build-dir-name> [ninja targets...]; SRC=<source tree> overrides the worktree.
# Run as: flock /tmp/box-build.lock bash build.sh ...
set -o pipefail
. "$(dirname "$0")/env.sh"
SRC=${SRC:-$WT}
TYPE=${1:?Release|Debug}
DIR=$B/${2:?dir}
shift 2
pgrep_guard
echo "FOREGROUND_START $(date -Is)"
export CCACHE_DISABLE=1
mkdir -p "$B" "$TMPDIR"
if [ ! -f "$DIR/build.ninja" ]; then
  cmake -S "$SRC" -B "$DIR" -G Ninja -DCMAKE_BUILD_TYPE="$TYPE" -DHVEN_FP_MODE=SAFER_FAST \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_EXAMPLES=ON \
    > "$DIR.configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -30 "$DIR.configure.log"; exit 1; }
fi
ninja -C "$DIR" -j12 -k 0 "$@" > "$DIR.build.log" 2>&1
rc=$?
tail -3 "$DIR.build.log"
echo "rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
