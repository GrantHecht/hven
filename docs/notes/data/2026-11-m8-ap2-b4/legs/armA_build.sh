#!/usr/bin/env bash
# AP-2 B4: build runtime arm A (f1117a86, the B3 landing = the B4 base) in a detached git worktree at
# build-b4/armA-src, Release, the build.sh configure flags, target hven_sqp_corpus only.
# Run as: flock /tmp/box-build.lock bash armA_build.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-ac366926f941a970a
SRC=$WT/build-b4/armA-src
DIR=$WT/build-b4/armA
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1
git -C "$SRC" log --oneline -1
cmake -S "$SRC" -B "$DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release -DHVEN_FP_MODE=SAFER_FAST \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE \
  -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_EXAMPLES=ON > "$DIR.configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -30 "$DIR.configure.log"; exit 1; }
ninja -C "$DIR" -j16 hven_sqp_corpus
rc=$?
echo "rc=$rc"
sha256sum "$DIR/bench/hven_sqp_corpus"
echo "FOREGROUND_END $(date -Is)"
exit $rc
