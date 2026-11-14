#!/usr/bin/env bash
# AP-2 B6 riders: arm A for the flags diff and the runtime gate. A detached worktree at the B6 code head
# 2a0392b9 (build-b6r/armA-src), configured Release with build.sh's flags into build-b6r/armA (the compile
# database the flags diff reads), and the runtime gate's arm A binary built there (target hven_sqp_corpus)
# and frozen at build-b6r/armAbin. Its sha256 is printed and compared with the B6 runtime gate's frozen
# arm B (the same commit, built in build-b6/rel): recorded either way, informational (the declaration
# pins arm A by the sha256 this leg prints, see runtime/DECLARATION.txt).
# Run as: flock /tmp/box-build.lock bash armA_config.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a79f97c30e3369bfe
SRC=$WT/build-b6r/armA-src
DIR=$WT/build-b6r/armA
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1
mkdir -p "$WT/build-b6r"
[ -d "$SRC" ] || git -C "$WT" worktree add --detach "$SRC" 2a0392b9 || { echo WORKTREE_FAIL; exit 1; }
git -C "$SRC" submodule update --init dep/eigen dep/fmt || { echo SUBMODULE_FAIL; exit 1; }
git -C "$SRC" submodule status
git -C "$SRC" log --oneline -1
[ "$(git -C "$SRC" rev-parse HEAD)" = "$(git -C "$WT" rev-parse 2a0392b9)" ] || { echo ARM_A_NOT_AT_2a0392b9; exit 1; }
cmake -S "$SRC" -B "$DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release -DHVEN_FP_MODE=SAFER_FAST \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE \
  -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_EXAMPLES=ON > "$DIR.configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -30 "$DIR.configure.log"; exit 1; }
ninja -C "$DIR" -j16 hven_sqp_corpus
rc=$?
echo "rc=$rc"
mkdir -p "$WT/build-b6r/armAbin"
cp -p "$DIR/bench/hven_sqp_corpus" "$WT/build-b6r/armAbin/hven_sqp_corpus" || rc=1
sha256sum "$WT/build-b6r/armAbin/hven_sqp_corpus" "$WT/build-b6/armBbin/hven_sqp_corpus"
echo "FOREGROUND_END $(date -Is)"
exit $rc
