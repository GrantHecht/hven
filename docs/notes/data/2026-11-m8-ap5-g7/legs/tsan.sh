#!/usr/bin/env bash
# M8 AP-5 G7 (../DECLARATION.md P8): copied from ../../2026-11-m8-ap5-g4/legs-r4/tsan.sh and re-pointed (worktree g7,
# build-g7/tsan, this directory); the drivers filter adds InterruptKinds.* (T11) and WarmProtocol.Sqp* is not a stop path.
# A separate tree (Release, -fsanitize=thread -g -fno-omit-frame-pointer on every TU and the link; no source change),
# targets hven_drivers_tests and hven_sqp_seam_tests. PASS: both runs exit 0 AND no "WARNING: ThreadSanitizer" line.
# usage: tsan.sh <evidence-dir> <raw-dir>      Run as: flock /tmp/box-build.lock bash tsan.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/g7
EV=${1:?evidence dir}
RAW=${2:?raw dir}
DIR=$WT/build-g7/tsan
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1 TMPDIR=$WT/build-g7/tmp GIT_CEILING_DIRECTORIES=$WT
mkdir -p "$TMPDIR" "$EV" "$RAW"
if [ ! -f "$DIR/build.ninja" ]; then
  cmake -S "$WT" -B "$DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release -DHVEN_FP_MODE=SAFER_FAST \
    -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
    -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/home/ghecht/Projects/hven/build-debug/_deps/googletest-src \
    "-DCMAKE_CXX_FLAGS=-fsanitize=thread -g -fno-omit-frame-pointer" \
    "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=thread" > "$RAW/tsan-configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -20 "$RAW/tsan-configure.log"; exit 1; }
fi
setsid -w ninja -C "$DIR" -j16 hven_drivers_tests hven_sqp_seam_tests > "$RAW/tsan-build.log" 2>&1 || { echo BUILD_FAIL; tail -20 "$RAW/tsan-build.log"; exit 1; }
tail -1 "$RAW/tsan-build.log"
FILTER='StopToken*:StopRecovery.*:Callback*:BestIterate.*:InterruptKinds.*'
TSAN_OPTIONS="halt_on_error=0 exitcode=66 history_size=7" setsid -w "$DIR/tests/drivers/hven_drivers_tests" \
  --gtest_filter="$FILTER" > "$RAW/tsan-run.log" 2>&1
rc=$?
tail -5 "$RAW/tsan-run.log" | tee "$EV/tsan-run-tail.txt"
TSAN_OPTIONS="halt_on_error=0 exitcode=66 history_size=7" setsid -w "$DIR/tests/hven_sqp_seam_tests" \
  --gtest_filter='StopPollSeam.*' > "$RAW/tsan-seam-run.log" 2>&1
rc2=$?
tail -5 "$RAW/tsan-seam-run.log" | tee "$EV/tsan-seam-run-tail.txt"
reports=$(cat "$RAW/tsan-run.log" "$RAW/tsan-seam-run.log" | grep -c 'WARNING: ThreadSanitizer')
echo "tsan rc=$rc seam rc=$rc2 reports=$reports" | tee -a "$EV/tsan-run-tail.txt"
verdict=FAIL; [ $rc -eq 0 ] && [ $rc2 -eq 0 ] && [ "$reports" = 0 ] && verdict=PASS
echo "TSAN_VERDICT $verdict" | tee -a "$EV/tsan-run-tail.txt"
echo "FOREGROUND_END $(date -Is)"
[ $verdict = PASS ]
