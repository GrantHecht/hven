# M8 AP-5 G4 r4 (../DECLARATION-r4.md): copied from ../legs-r3/tsan.sh with -r3 -> -r4 and g4r3 -> g4r4 in code lines only; nothing else.
# M8 AP-5 G4 r3 (../DECLARATION-r3.md): copied from ../legs/tsan.sh; changes: builds hven_sqp_seam_tests too; drivers filter adds BestIterate.*; a second run of the seam binary with StopPollSeam.*; PASS needs both exit 0 and 0 reports. Older provenance lines below are historical.
# M8 AP-5 G4 (../DECLARATION.md): copied from ../../2026-11-m8-ap5-g2/ and re-pointed in code lines only (this worktree, build-g5/, this directory); older provenance lines below are historical.
# M8 AP-5 G2 (../DECLARATION.md): copied from ../../2026-11-m8-ap5-g1/ (legs-r3 / runtime-r3) and re-pointed in code lines only (worktree agent-a41fdd4ad89efc301, build-g2/, this directory); replay.sh, interior.sh and chain.sh are changed as the declaration says; older provenance lines below are historical.
#!/usr/bin/env bash
# M8 AP-5 G1 r2 (../DECLARATION-r3.txt): copied from tsan.sh (run 1 failed at configure: the C compiler defaulted to
# /usr/bin/cc = GCC, whose libtsan is not installed) with ONE change, -DCMAKE_C_COMPILER=/usr/bin/clang, and its own
# tree build-g2/tsan. Was: M8 AP-5 G1 (../DECLARATION.txt P8): the local ThreadSanitizer run of the cross-thread stop tests (no CI lane). A
# separate build tree build-g2/tsan (Release, -fsanitize=thread -g -fno-omit-frame-pointer on every TU and the link;
# no source change), target hven_drivers_tests only, then the T2 and T10 cases and the token's own unit tests.
# PASS: the test binary exits 0 AND the log carries no "WARNING: ThreadSanitizer" line.
# usage: tsan.sh <evidence-dir>      Run as: flock /tmp/box-build.lock bash tsan.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/g5
EV=${1:?evidence dir}
DIR=$WT/build-g5/tsan
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1 TMPDIR=$WT/build-g5/tmp
mkdir -p "$TMPDIR" "$EV"
if [ ! -f "$DIR/build.ninja" ]; then
  cmake -S "$WT" -B "$DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release -DHVEN_FP_MODE=SAFER_FAST \
    -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
    "-DCMAKE_CXX_FLAGS=-fsanitize=thread -g -fno-omit-frame-pointer" \
    "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=thread" > "$EV/tsan-g5-configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -20 "$EV/tsan-g5-configure.log"; exit 1; }
fi
ninja -C "$DIR" -j16 hven_drivers_tests hven_sqp_seam_tests > "$EV/tsan-g5-build.log" 2>&1 || { echo BUILD_FAIL; tail -20 "$EV/tsan-g5-build.log"; exit 1; }
tail -1 "$EV/tsan-g5-build.log"
FILTER='StopToken*:StopRecovery.*:Callback*:BestIterate.*'
TSAN_OPTIONS="halt_on_error=0 exitcode=66 history_size=7" setsid "$DIR/tests/drivers/hven_drivers_tests" \
  --gtest_filter="$FILTER" > "$EV/tsan-g5-run.log" 2>&1
rc=$?
tail -5 "$EV/tsan-g5-run.log"
TSAN_OPTIONS="halt_on_error=0 exitcode=66 history_size=7" setsid "$DIR/tests/hven_sqp_seam_tests" \
  --gtest_filter='StopPollSeam.*' > "$EV/tsan-g5-seam-run.log" 2>&1
rc2=$?
tail -5 "$EV/tsan-g5-seam-run.log"
reports=$(cat "$EV/tsan-g5-run.log" "$EV/tsan-g5-seam-run.log" | grep -c 'WARNING: ThreadSanitizer')
echo "tsan rc=$rc seam rc=$rc2 reports=$reports"
verdict=FAIL; [ $rc -eq 0 ] && [ $rc2 -eq 0 ] && [ "$reports" = 0 ] && verdict=PASS
echo "TSAN_VERDICT $verdict"
echo "FOREGROUND_END $(date -Is)"
[ $verdict = PASS ]
