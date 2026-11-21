#!/usr/bin/env bash
# AP-2 B8 riders (copied from B8, re-pointed build-b8 -> build-b8r) build leg (B5's instrument).
# usage: build.sh <Release|Debug> <build-dir-name> [ninja targets...]
#   SRC=<source dir> (optional; default this worktree) -- arm A (cf164dcf) is built from its own
#   `git archive` export under build-b8r/armA-src.
# Run as: flock /tmp/box-build.lock bash build.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-aad152e17d08967f4
SRCDIR=${SRC:-$WT}
TYPE=${1:?Release|Debug}
DIR=$WT/build-b8r/${2:?dir}
shift 2
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
echo "source $SRCDIR"
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1
mkdir -p "$WT/build-b8r"
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
