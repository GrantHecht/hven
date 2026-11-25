#!/usr/bin/env bash
# usage: build_ex.sh <dir-name>: re-configure an existing riders build dir with bench + examples ON (the declared PF-1 suite's regime, 3483 tests) and build all.
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf1-riders
DIR=$WT/build-riders/${1:?dir}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo PGREP_BUSY; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1
cmake -S "$WT" -B "$DIR" -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_EXAMPLES=ON > "$DIR.configure-ex.log" 2>&1 || { echo CONFIGURE_FAIL; tail -30 "$DIR.configure-ex.log"; exit 1; }
ninja -C "$DIR" -j16
rc=$?
echo "rc=$rc"; echo "FOREGROUND_END $(date -Is)"; exit $rc
