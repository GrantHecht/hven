#!/usr/bin/env bash
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf1b
DIR=$WT/build-pf1b/diag
export CCACHE_DISABLE=1
if [ ! -f "$DIR/build.ninja" ]; then
  cmake -S "$WT/build-pf1b/diag-src" -B "$DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release -DHVEN_FP_MODE=SAFER_FAST \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_EXAMPLES=OFF > "$DIR.configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -30 "$DIR.configure.log"; exit 1; }
fi
ninja -C "$DIR" -j16 "$@" 2>&1 | tail -15
