#!/usr/bin/env bash
# Not a leg: bring build-se/rel and build-se/dbg to the code head, check the bench stamp, freeze arm B's corpus binary, build arm A's
# (the detached base worktree), then both lane binaries (train_setup.sh). usage: prebuild.sh <code sha> <base sha>
#   Run under: flock /tmp/box-build.lock setsid -w bash prebuild.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/sparse-equil
BASE=/home/ghecht/Projects/hven/.claude/worktrees/sparse-equil-base
L=$WT/docs/notes/data/2026-11-m8-sparse-equil/legs
CODE=${1:?code sha}; BASESHA=${2:?base sha}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
[ "$(git -C "$WT" rev-parse HEAD)" = "$CODE" ] || { echo HEAD_NOT_CODE; exit 2; }
[ "$(git -C "$BASE" rev-parse HEAD)" = "$BASESHA" ] || { echo BASE_HEAD_WRONG; exit 2; }
[ -z "$(git -C "$WT" status --porcelain --untracked-files=no)" ] || { echo TREE_DIRTY; exit 2; }
bash "$L/build.sh" "$WT" Release rel || exit 1
bash "$L/build.sh" "$WT" Debug dbg || exit 1
ninja -C "$WT/build-se/rel" hven_bench_stamp_check || { echo STAMP_STALE; exit 1; }
mkdir -p "$WT/build-se/bin/A" "$WT/build-se/bin/B"
cp -p "$WT/build-se/rel/bench/hven_sqp_corpus" "$WT/build-se/bin/B/hven_sqp_corpus" || exit 1
bash "$L/build.sh" "$BASE" Release rel hven_sqp_corpus || exit 1
cp -p "$BASE/build-se/rel/bench/hven_sqp_corpus" "$WT/build-se/bin/A/hven_sqp_corpus" || exit 1
sha256sum "$WT/build-se/bin/A/hven_sqp_corpus" "$WT/build-se/bin/B/hven_sqp_corpus"
bash "$L/train_setup.sh" A "$BASESHA" || exit 1
bash "$L/train_setup.sh" B "$CODE" || exit 1
sha256sum "$WT/build-se/bin/A/hven_bench_cell" "$WT/build-se/bin/B/hven_bench_cell"
echo "FOREGROUND_END $(date -Is)"
