#!/usr/bin/env bash
# AP-2 B7: arm A for the flags diff, the runtime gate and the interior leg. A detached worktree at the B7
# base 31c73879 (m8 after B6; build-b7e/armA-src), configured Release with build.sh's flags into
# build-b7e/armA (the compile database the flags diff reads), and the arm A corpus binary built there
# (target hven_sqp_corpus) and frozen at build-b7e/armAbin. Its sha256 is printed; the declaration pins
# arm A by it (run before the declaration, as B6's was).
# Run as: flock /tmp/box-build.lock bash armA_config.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a00603e2048c1c37b
SRC=$WT/build-b7e/armA-src
DIR=$WT/build-b7e/armA
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1
mkdir -p "$WT/build-b7e"
[ -d "$SRC" ] || git -C "$WT" worktree add --detach "$SRC" 31c73879 || { echo WORKTREE_FAIL; exit 1; }
git -C "$SRC" submodule update --init dep/eigen dep/fmt || { echo SUBMODULE_FAIL; exit 1; }
git -C "$SRC" submodule status
git -C "$SRC" log --oneline -1
[ "$(git -C "$SRC" rev-parse HEAD)" = "$(git -C "$WT" rev-parse 31c73879)" ] || { echo ARM_A_NOT_AT_31c73879; exit 1; }
cmake -S "$SRC" -B "$DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release -DHVEN_FP_MODE=SAFER_FAST \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE \
  -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_EXAMPLES=ON > "$DIR.configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -30 "$DIR.configure.log"; exit 1; }
ninja -C "$DIR" -j16 hven_sqp_corpus
rc=$?
echo "rc=$rc"
mkdir -p "$WT/build-b7e/armAbin"
cp -p "$DIR/bench/hven_sqp_corpus" "$WT/build-b7e/armAbin/hven_sqp_corpus" || rc=1
sha256sum "$WT/build-b7e/armAbin/hven_sqp_corpus"
echo "FOREGROUND_END $(date -Is)"
exit $rc
