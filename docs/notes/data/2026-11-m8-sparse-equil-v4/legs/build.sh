#!/usr/bin/env bash
# sparse-equil v4: copied from ../../2026-11-m8-kopt-1x-r2/ (same relative path), re-pointed to this worktree and evidence directory; older provenance lines below are historical.
# usage: build.sh <worktree> <Release|Debug> <build-dir-name> [ninja targets...]   Run under: flock /tmp/box-build.lock bash build.sh ...
set -o pipefail
WT=${1:?worktree}; TYPE=${2:?Release|Debug}; DIR=$WT/build-k/${3:?dir}; shift 3
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1 TMPDIR=$WT/build-k/tmp
mkdir -p "$WT/build-k" "$TMPDIR"
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
