#!/usr/bin/env bash
# M8 dense gate (../DECLARATION.md, before the declaration): bring build-dg/rel and build-dg/dbg to the code head (the
# bench stamp re-configures on the commit), check the stamp, freeze the corpus binary to build-dg/dgbin/hven_sqp_corpus,
# then the lane binary (../sl/lane_setup.sh). Not a leg.
# usage: prebuild.sh <code head sha>   Run as: flock /tmp/box-build.lock bash prebuild.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a44d4824b0beb0ea7
L=$WT/docs/notes/data/2026-11-m8-dense-gate/legs
HEADSHA=${1:?code head sha}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
[ "$(git -C "$WT" rev-parse HEAD)" = "$HEADSHA" ] || { echo HEAD_NOT_CODE_HEAD; exit 2; }
[ -z "$(git -C "$WT" status --porcelain --untracked-files=no)" ] || { echo TREE_DIRTY; exit 2; }
bash "$L/build.sh" Release rel || exit 1
bash "$L/build.sh" Debug dbg || exit 1
ninja -C "$WT/build-dg/rel" hven_bench_stamp_check || { echo STAMP_STALE; exit 1; }
find "$WT/build-dg/rel" -name hven_bench_stamp.txt -exec cat {} \;
mkdir -p "$WT/build-dg/dgbin"
cp -p "$WT/build-dg/rel/bench/hven_sqp_corpus" "$WT/build-dg/dgbin/hven_sqp_corpus" || exit 1
sha256sum "$WT/build-dg/dgbin/hven_sqp_corpus"
bash "$WT/docs/notes/data/2026-11-m8-dense-gate/sl/lane_setup.sh" "$HEADSHA" dense-gate-legs dgbin || exit 1
echo "FOREGROUND_END $(date -Is)"
