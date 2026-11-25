#!/usr/bin/env bash
# PF-4 r2 (../../DECLARATION-PF-4-r2.md): copy of ../../legs/build.sh re-pointed to the pf4-r2 worktree and r2/ (paths only); older lines are historical.
# PF-4: ../../2026-11-m8-wa1/legs/build.sh with the source tree an argument (the worktree for arm B, the `git archive` copy of
# origin/m8 for arm A) and the trees under .scratch/build-pf4/.
# usage: build.sh <source-dir> <Release|Debug> <build-dir-name> [ninja targets...]
#   Run under (r2): ~/.local/bin/box-lane -- bash build.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf4-r2
SRC=${1:?source}; TYPE=${2:?Release|Debug}; DIR=$WT/.scratch/build-pf4/${3:?dir}; shift 3
[ -n "${BOX_LANE:-}" ] || { echo "NOT_IN_BOX_LANE (r2: every leg runs inside ~/.local/bin/box-lane)"; exit 3; }; echo "BOX_LANE=$BOX_LANE phys=[$BOX_LANE_PHYS] jobs=$BOX_LANE_JOBS"
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1 TMPDIR=$WT/.scratch/tmp
mkdir -p "$WT/.scratch/build-pf4" "$TMPDIR"
if [ ! -f "$DIR/build.ninja" ]; then
  cmake -S "$SRC" -B "$DIR" -G Ninja -DCMAKE_BUILD_TYPE="$TYPE" -DHVEN_FP_MODE=SAFER_FAST \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE \
    -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_EXAMPLES=ON > "$DIR.configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -30 "$DIR.configure.log"; exit 1; }
fi
ninja -C "$DIR" -j"${BOX_LANE_JOBS:-4}" -k 0 "$@" > "$DIR.build.log" 2>&1
rc=$?
tail -3 "$DIR.build.log"
echo "rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
