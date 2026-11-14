#!/usr/bin/env bash
# M8 PF-1, copied from PF-HB (which copied the B8 riders) and re-pointed (worktree, build-pf1e, this directory); was: AP-2 B8 riders (copied from B7 riders, re-pointed) (the B3/B4 instrument), settler C222 addition: the off-reference CI-flag build. The linux-clang-release
# preset with -DHVEN_TEST_ROUTE_REFERENCE_DISPATCH=OFF, HVEN_BUILD_BENCH=ON and
# HVEN_BUILD_EXAMPLES=ON, into build-pf1e/offref (the preset's binaryDir overridden with -B),
# built clean, then ctest Release (-j4 under setsid).
# usage: offref.sh <evidence-dir>   Run as: flock /tmp/box-build.lock bash offref.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-aa2c5c3a798557d0e
EV=${1:?evidence dir}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1
cd "$WT" || exit 1
mkdir -p "$EV"
DIR=$WT/build-pf1e/offref
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
