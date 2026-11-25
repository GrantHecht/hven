#!/usr/bin/env bash
# M8 AP-5 G3 (../DECLARATION-r4.md): adapted from ../../2026-11-m8-ap5-g2/ (legs-r4, runtime); see the declaration for what differs.
# P8: the local ThreadSanitizer run (no CI lane). A separate tree build-g3/tsan (Release, -fsanitize=thread -g
# -fno-omit-frame-pointer on every TU and the link; clang as the C compiler, GCC's libtsan is not installed), target
# hven_drivers_tests only, every stop, callback and best-iterate test in the binary.
# PASS: the test binary exits 0 AND the log carries no "WARNING: ThreadSanitizer" line.
# usage: tsan.sh <evidence-dir>      Run as: flock /tmp/box-build.lock bash tsan.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-ac15f1e91e3a3b6b2
EV=${1:?evidence dir}
DIR=$WT/build-g3/tsan
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1 TMPDIR=$WT/build-g3/tmp
mkdir -p "$TMPDIR" "$EV"
if [ ! -f "$DIR/build.ninja" ]; then
  cmake -S "$WT" -B "$DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release -DHVEN_FP_MODE=SAFER_FAST \
    -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
    "-DCMAKE_CXX_FLAGS=-fsanitize=thread -g -fno-omit-frame-pointer" \
    "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=thread" > "$EV/tsan-configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -20 "$EV/tsan-configure.log"; exit 1; }
fi
ninja -C "$DIR" -j16 hven_drivers_tests > "$EV/tsan-build.log" 2>&1 || { echo BUILD_FAIL; tail -20 "$EV/tsan-build.log"; exit 1; }
tail -1 "$EV/tsan-build.log"
FILTER='StopToken*:StopRecovery.*:Callback*:BestIterate.*'
TSAN_OPTIONS="halt_on_error=0 exitcode=66 history_size=7" setsid "$DIR/tests/drivers/hven_drivers_tests" \
  --gtest_filter="$FILTER" > "$EV/tsan-run.log" 2>&1
rc=$?
tail -5 "$EV/tsan-run.log"
reports=$(grep -c 'WARNING: ThreadSanitizer' "$EV/tsan-run.log")
echo "tsan rc=$rc reports=$reports"
verdict=FAIL; [ $rc -eq 0 ] && [ "$reports" = 0 ] && verdict=PASS
echo "TSAN_VERDICT $verdict"
echo "FOREGROUND_END $(date -Is)"
[ $verdict = PASS ]
