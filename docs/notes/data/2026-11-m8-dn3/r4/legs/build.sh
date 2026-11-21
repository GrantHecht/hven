#!/usr/bin/env bash
# M8 DN-3 r4 (../../DECLARATION-r4.md): ../../legs/build.sh re-pointed (worktree agent-a739270f237b3e30c, build-flip/).
# usage: build.sh <Release|Debug> <build-dir-name> [ninja targets...]   Run as: flock /tmp/box-build.lock bash build.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a739270f237b3e30c
TYPE=${1:?Release|Debug}
DIR=$WT/build-flip/${2:?dir}
shift 2
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1 TMPDIR=$WT/build-flip/tmp
mkdir -p "$WT/build-flip" "$TMPDIR"
if [ ! -f "$DIR/build.ninja" ]; then
  cmake -S "$WT" -B "$DIR" -G Ninja -DCMAKE_BUILD_TYPE="$TYPE" -DHVEN_FP_MODE=SAFER_FAST \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE \
    -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_EXAMPLES=ON > "$DIR.configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -30 "$DIR.configure.log"; exit 1; }
fi
ninja -C "$DIR" -j16 -k 0 "$@" > "$DIR.build.log" 2>&1
rc=$?
tail -3 "$DIR.build.log"
echo "rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
