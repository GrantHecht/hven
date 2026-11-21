#!/usr/bin/env bash
# AP-2 B2: build the PIN (origin/m8 = 03d5d121) unchanged, Release, from an
# exported source copy under build-b2/pin-src, for the runtime protocol's arm A.
# Run as: flock /tmp/box-build.lock bash build_pin.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-aba3deeb97313f444
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1
cd "$WT" || exit 1
SRC=$WT/build-b2/pin-src
rm -rf "$SRC" && mkdir -p "$SRC"
git archive 03d5d121 | tar -x -C "$SRC" || exit 1
cp -a dep/eigen dep/fmt "$SRC/dep/" || exit 1
cmake -S "$SRC" -B "$WT/build-b2/pin" -G Ninja -DCMAKE_BUILD_TYPE=Release -DHVEN_FP_MODE=SAFER_FAST \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE -DHVEN_BUILD_BENCH=ON \
  -DHVEN_BUILD_EXAMPLES=ON > "$WT/build-b2/pin-configure.log" 2>&1 || { echo CONFIGURE_FAIL; exit 1; }
ninja -C "$WT/build-b2/pin" -j16 hven_sqp_corpus > "$WT/build-b2/pin-build.log" 2>&1
rc=$?
echo "rc=$rc"
mkdir -p "$WT/build-b2/armA" && cp -p "$WT/build-b2/pin/bench/hven_sqp_corpus" "$WT/build-b2/armA/" && sha256sum "$WT/build-b2/armA/hven_sqp_corpus"
echo "FOREGROUND_END $(date -Is)"
exit $rc
