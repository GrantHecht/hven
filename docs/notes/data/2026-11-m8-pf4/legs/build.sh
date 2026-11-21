#!/usr/bin/env bash
# PF-4: ../../2026-11-m8-wa1/legs/build.sh with the source tree an argument (the worktree for arm B, the `git archive` copy of
# origin/m8 for arm A) and the trees under .scratch/build-pf4/.
# usage: build.sh <source-dir> <Release|Debug> <build-dir-name> [ninja targets...]
#   Run under: setsid -w flock /tmp/box-build.lock bash build.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf4
SRC=${1:?source}; TYPE=${2:?Release|Debug}; DIR=$WT/.scratch/build-pf4/${3:?dir}; shift 3
if [ -n "$(pgrep -x hven_bench_cell)" ]; then echo "PGREP_BUSY"; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1 TMPDIR=$WT/.scratch/tmp
mkdir -p "$WT/.scratch/build-pf4" "$TMPDIR"
if [ ! -f "$DIR/build.ninja" ]; then
  cmake -S "$SRC" -B "$DIR" -G Ninja -DCMAKE_BUILD_TYPE="$TYPE" -DHVEN_FP_MODE=SAFER_FAST \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE \
    -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_EXAMPLES=ON > "$DIR.configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -30 "$DIR.configure.log"; exit 1; }
fi
ninja -C "$DIR" -j16 -k 0 "$@" > "$DIR.build.log" 2>&1
rc=$?
tail -3 "$DIR.build.log"
echo "rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
