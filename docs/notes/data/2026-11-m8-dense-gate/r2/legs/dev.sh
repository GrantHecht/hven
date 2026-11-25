#!/usr/bin/env bash
# M8 dense gate r2 (before DECLARATION-r2.md; not a leg, disclosed there): prebuild at the rebased code head, then the
# dev readings the declaration's criteria are derived from -- Release ctest count, one replay pass, one interior pass,
# and the dev TRAIN run (every roster cell x {default, never-default}) tallied against the walk cap's leg rows.
# usage: dev.sh <code head sha>   Run as: flock /tmp/box-build.lock bash dev.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a44d4824b0beb0ea7
DG=$WT/docs/notes/data/2026-11-m8-dense-gate/r2
HEADSHA=${1:?code head sha}
mkdir -p "$DG/dev"
bash "$DG/legs/prebuild.sh" "$HEADSHA" > "$DG/dev/prebuild.out" 2>&1 || { echo PREBUILD_FAIL; tail -20 "$DG/dev/prebuild.out"; exit 1; }
tail -8 "$DG/dev/prebuild.out"
CORPUS=$(sha256sum "$WT/build-dg/dgbin-r2/hven_sqp_corpus" | cut -d' ' -f1)
LANE=$(sha256sum "$WT/build-dg/dgbin-r2/hven_bench_cell" | cut -d' ' -f1)
echo "corpus $CORPUS lane $LANE"
bash "$DG/legs/ctest.sh" rel "$WT/build-dg/out/r2dev-ctest-rel.log" > "$DG/dev/ctest-rel.out" 2>&1
grep -E "tests passed|\(Failed\)" "$WT/build-dg/out/r2dev-ctest-rel.log" | head
bash "$DG/legs/replay.sh" "$WT/build-dg/out/r2dev-replay" "$CORPUS" > "$DG/dev/replay.out" 2>&1; echo "dev replay rc=$?"
grep -E "compare rc|differences" "$DG/dev/replay.out" | head -20
bash "$DG/legs/interior.sh" "$WT/build-dg/out/r2dev-interior" "$CORPUS" > "$DG/dev/interior.out" 2>&1; echo "dev interior rc=$?"
tail -5 "$DG/dev/interior.out"
: > "$DG/dev/none.txt"
bash "$DG/sl/enum.sh" "$DG/dev/identity" "$LANE" "$DG/dev/none.txt" > "$DG/dev/enum.out" 2>&1; echo "dev enum rc=$?"
tail -40 "$DG/dev/enum.out"
