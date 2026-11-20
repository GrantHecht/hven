#!/usr/bin/env bash
# PF-5 prototype build (scratch). Run under: setsid -w flock /tmp/box-build.lock bash build.sh <targets...>
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf5-proto
DIR=$WT/.scratch/rel
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1 TMPDIR=$WT/.scratch/tmp
echo "FOREGROUND_START $(date -Is)"
if [ ! -f "$DIR/build.ninja" ]; then
  cmake -S "$WT" -B "$DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release -DHVEN_FP_MODE=SAFER_FAST \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DHVEN_BUILD_BENCH=ON > "$DIR.configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -30 "$DIR.configure.log"; exit 1; }
fi
ninja -C "$DIR" -j${BOX_LANE_JOBS:-4} "$@" > "$DIR.build.log" 2>&1
rc=$?; tail -3 "$DIR.build.log"; echo "rc=$rc"; echo "FOREGROUND_END $(date -Is)"; exit $rc
