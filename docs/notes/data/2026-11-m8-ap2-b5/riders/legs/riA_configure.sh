#!/usr/bin/env bash
# AP-2 B5 riders: CONFIGURE ONLY (no build) the B5 code head 2142af0a from the detached worktree
# build-b5/riA-src into build-b5/riA, with build.sh's configure flags, for the compile-flags diff's
# arm A compile database. Run as: flock /tmp/box-build.lock bash riA_configure.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a17ce5e51d9c90f96
SRC=$WT/build-b5/riA-src
DIR=$WT/build-b5/riA
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
git -C "$SRC" log --oneline -1
rm -rf "$DIR"
cmake -S "$SRC" -B "$DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release -DHVEN_FP_MODE=SAFER_FAST \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE \
  -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_EXAMPLES=ON > "$DIR.configure.log" 2>&1
rc=$?
[ $rc -ne 0 ] && tail -30 "$DIR.configure.log"
echo "rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
