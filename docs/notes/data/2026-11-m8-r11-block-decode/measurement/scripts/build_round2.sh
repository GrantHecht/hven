#!/usr/bin/env bash
# R11 scratch round 2: meas rebuilt with probe-v2; TRAIN harness build; worktree dev Release build (build-r11/dev) + the dense factor tests.
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/r11
B=$WT/build-r11
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo PGREP_BUSY; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1
ninja -C "$B/meas" -j16 hven_sqp_corpus > "$B/meas2.build.log" 2>&1; echo "meas v2 rc=$?"; tail -1 "$B/meas2.build.log"
nm -C "$B/meas/bench/hven_sqp_corpus" | grep -c "r11::maybe_snapshot"
if [ ! -f "$B/dev/build.ninja" ]; then
  cmake -S "$WT" -B "$B/dev" -G Ninja -DCMAKE_BUILD_TYPE=Release -DHVEN_FP_MODE=SAFER_FAST \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_EXAMPLES=ON > "$B/dev.configure.log" 2>&1 || { echo DEV_CONFIGURE_FAIL; exit 1; }
fi
ninja -C "$B/dev" -j16 > "$B/dev.build.log" 2>&1; echo "dev rc=$?"; grep -E "error|warning:" "$B/dev.build.log" | head -20
"$B/dev/tests/hven_tests" --gtest_filter='DenseSymmetricFactor*:Schur*:DenseSymmetric*' 2>&1 | tail -15
FOREGROUND_END_MARK=1
echo "FOREGROUND_END $(date -Is)"
