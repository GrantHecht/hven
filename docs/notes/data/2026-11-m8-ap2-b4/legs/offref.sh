#!/usr/bin/env bash
# AP-2 B4 (the B3 instrument), settler C222 addition: the off-reference CI-flag build. The linux-clang-release
# preset with -DHVEN_TEST_ROUTE_REFERENCE_DISPATCH=OFF, HVEN_BUILD_BENCH=ON and
# HVEN_BUILD_EXAMPLES=ON, into build-b4/offref (the preset's binaryDir overridden with -B),
# built clean, then ctest Release (-j4 under setsid).
# usage: offref.sh <evidence-dir>   Run as: flock /tmp/box-build.lock bash offref.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-ac366926f941a970a
EV=${1:?evidence dir}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1
cd "$WT" || exit 1
mkdir -p "$EV"
DIR=$WT/build-b4/offref
rm -rf "$DIR"
cmake --preset linux-clang-release -B "$DIR" -DHVEN_TEST_ROUTE_REFERENCE_DISPATCH=OFF \
  -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_EXAMPLES=ON > "$EV/configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -20 "$EV/configure.log"; exit 1; }
ninja -C "$DIR" -j16 > "$EV/build.log" 2>&1
rc=$?
echo "build rc=$rc"; grep -cE 'warning:' "$EV/build.log"; tail -2 "$EV/build.log"
[ $rc -ne 0 ] && { grep -E 'error' "$EV/build.log" | head -40; echo "FOREGROUND_END $(date -Is)"; exit $rc; }
cd "$DIR" || exit 1
setsid ctest -j4 --output-on-failure > "$EV/ctest-release.log" 2>&1
rc=$?
tail -5 "$EV/ctest-release.log"
echo "ctest rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
