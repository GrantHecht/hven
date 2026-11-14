#!/usr/bin/env bash
# M8 walk cap (../DECLARATION.md, before the declaration; not a leg): ../../2026-11-m8-dn3/r4/legs/prebuild.sh
# re-pointed. Bring build-wc/rel and build-wc/dbg to the code commit (the bench stamp re-configures on the commit),
# check the stamp, freeze the corpus binary to build-wc/bin/hven_sqp_corpus, then the lane binary (train_setup.sh).
# usage: prebuild.sh <code sha>   Run as: flock /tmp/box-build.lock bash prebuild.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a7a2ce81cf8988de5
L=$WT/docs/notes/data/2026-11-m8-walk-cap/legs
CODE=${1:?code sha}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
[ "$(git -C "$WT" rev-parse HEAD)" = "$CODE" ] || { echo HEAD_NOT_CODE; exit 2; }
[ -z "$(git -C "$WT" status --porcelain --untracked-files=no)" ] || { echo TREE_DIRTY; exit 2; }
bash "$L/build.sh" Release rel || exit 1
bash "$L/build.sh" Debug dbg || exit 1
ninja -C "$WT/build-wc/rel" hven_bench_stamp_check || { echo STAMP_STALE; exit 1; }
find "$WT/build-wc/rel" -name hven_bench_stamp.txt -exec cat {} \;
mkdir -p "$WT/build-wc/bin"
cp -p "$WT/build-wc/rel/bench/hven_sqp_corpus" "$WT/build-wc/bin/hven_sqp_corpus" || exit 1
sha256sum "$WT/build-wc/bin/hven_sqp_corpus"
bash "$L/train_setup.sh" "$CODE" m8-walk-cap-legs || exit 1
echo "FOREGROUND_END $(date -Is)"
