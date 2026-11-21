#!/usr/bin/env bash
# M8 DN-3 r4 (../../DECLARATION-r4.md, section 2, before the declaration): bring build-flip/rel and build-flip/dbg to the
# flip commit (the bench stamp re-configures on the commit), check the stamp, freeze the corpus binary to
# build-flip/flipbin/hven_sqp_corpus, then the lane binary (../sl/flip_setup.sh). Not a leg.
# usage: prebuild.sh <flip sha>   Run as: flock /tmp/box-build.lock bash prebuild.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a739270f237b3e30c
L=$WT/docs/notes/data/2026-11-m8-dn3/r4/legs
FLIP=${1:?flip sha}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
[ "$(git -C "$WT" rev-parse HEAD)" = "$FLIP" ] || { echo HEAD_NOT_FLIP; exit 2; }
[ -z "$(git -C "$WT" status --porcelain --untracked-files=no)" ] || { echo TREE_DIRTY; exit 2; }
bash "$L/build.sh" Release rel || exit 1
bash "$L/build.sh" Debug dbg || exit 1
ninja -C "$WT/build-flip/rel" hven_bench_stamp_check || { echo STAMP_STALE; exit 1; }
cat "$WT/build-flip/rel/hven_bench_stamp.txt" 2>/dev/null || find "$WT/build-flip/rel" -name hven_bench_stamp.txt -exec cat {} \;
mkdir -p "$WT/build-flip/flipbin"
cp -p "$WT/build-flip/rel/bench/hven_sqp_corpus" "$WT/build-flip/flipbin/hven_sqp_corpus" || exit 1
sha256sum "$WT/build-flip/flipbin/hven_sqp_corpus"
bash "$WT/docs/notes/data/2026-11-m8-dn3/r4/sl/flip_setup.sh" "$FLIP" dn3-flip-legs flipbin || exit 1
echo "FOREGROUND_END $(date -Is)"
