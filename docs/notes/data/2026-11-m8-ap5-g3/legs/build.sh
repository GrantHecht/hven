#!/usr/bin/env bash
# M8 AP-5 G3 (../DECLARATION.md): adapted from ../../2026-11-m8-ap5-g2/ (legs-r3, runtime); see the declaration for what differs.
# usage: build.sh <Release|Debug> <build-dir-name> [ninja targets...]
# Run as: flock /tmp/box-build.lock bash build.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-ac15f1e91e3a3b6b2
SRCDIR=${SRC:-$WT}
TYPE=${1:?Release|Debug}
DIR=${BUILD_ROOT:-$WT/build-g3}/${2:?dir}   # BUILD_ROOT: the dev falsifier only (scratch)
shift 2
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
echo "source $SRCDIR"
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1 TMPDIR=$WT/build-g3/tmp
mkdir -p "$TMPDIR"
mkdir -p "${BUILD_ROOT:-$WT/build-g3}"
if [ ! -f "$DIR/build.ninja" ]; then
  cmake -S "$SRCDIR" -B "$DIR" -G Ninja -DCMAKE_BUILD_TYPE="$TYPE" -DHVEN_FP_MODE=SAFER_FAST \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE \
    -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_EXAMPLES=ON > "$DIR.configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -30 "$DIR.configure.log"; exit 1; }
fi
ninja -C "$DIR" -j16 -k 0 "$@"
rc=$?
echo "rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
