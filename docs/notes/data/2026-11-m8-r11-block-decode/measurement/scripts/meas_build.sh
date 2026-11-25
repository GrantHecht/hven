#!/usr/bin/env bash
# R11 scratch measurement build (probe-instrumented export of a8c540cf7). Not evidence of the proof chain.
set -o pipefail
B=/home/ghecht/Projects/hven/.claude/worktrees/r11/build-r11
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo PGREP_BUSY; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1
if [ ! -f "$B/meas/build.ninja" ]; then
  cmake -S "$B/meas-src" -B "$B/meas" -G Ninja -DCMAKE_BUILD_TYPE=Release -DHVEN_FP_MODE=SAFER_FAST \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_EXAMPLES=OFF > "$B/meas.configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -20 "$B/meas.configure.log"; exit 1; }
fi
ninja -C "$B/meas" -j16 hven_sqp_corpus "$@"
rc=$?
echo "rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
