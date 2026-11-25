#!/usr/bin/env bash
# M8 DN-3 r5 (../../DECLARATION-r5.md, before the declaration): ../../r4/legs/prebuild.sh re-pointed to the r5 code head
# without the lane step (the lane binary is r4's: no library source moved). r4's frozen corpus binary is kept as
# build-flip/flipbin-r4/hven_sqp_corpus; the r5 one replaces it at build-flip/flipbin/hven_sqp_corpus. Not a leg.
# usage: prebuild.sh <code head sha>   Run as: flock /tmp/box-build.lock bash prebuild.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a739270f237b3e30c
L4=$WT/docs/notes/data/2026-11-m8-dn3/r4/legs
HEADSHA=${1:?code head sha}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
[ "$(git -C "$WT" rev-parse HEAD)" = "$HEADSHA" ] || { echo HEAD_NOT_CODE_HEAD; exit 2; }
[ -z "$(git -C "$WT" status --porcelain --untracked-files=no)" ] || { echo TREE_DIRTY; exit 2; }
mkdir -p "$WT/build-flip/flipbin-r4"
[ -f "$WT/build-flip/flipbin-r4/hven_sqp_corpus" ] || cp -p "$WT/build-flip/flipbin/hven_sqp_corpus" "$WT/build-flip/flipbin-r4/hven_sqp_corpus" || exit 1
sha256sum "$WT/build-flip/flipbin-r4/hven_sqp_corpus"
bash "$L4/build.sh" Release rel || exit 1
bash "$L4/build.sh" Debug dbg || exit 1
ninja -C "$WT/build-flip/rel" hven_bench_stamp_check || { echo STAMP_STALE; exit 1; }
cp -p "$WT/build-flip/rel/bench/hven_sqp_corpus" "$WT/build-flip/flipbin/hven_sqp_corpus" || exit 1
sha256sum "$WT/build-flip/flipbin/hven_sqp_corpus" "$WT/build-flip/flipbin/hven_bench_cell"
echo "FOREGROUND_END $(date -Is)"
