#!/usr/bin/env bash
# M8 AP-5 G1 r2 (../DECLARATION-r2.txt): chain.sh's source guards, then P8 only, through legs/tsan-r2.sh.
# usage: chain-r2.sh <code-head sha>   Run as: flock /tmp/box-build.lock bash chain-r2.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-afb4015ceb50aa1d1
D=$WT/docs/notes/data/2026-11-m8-ap5-g1
L=$D/legs
HEAD_SHA=${1:?code head sha}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
git -C "$WT" diff --quiet "$HEAD_SHA" -- src include tests bench examples CMakeLists.txt cmake scripts || { echo "SOURCE_MOVED_SINCE_CODE_HEAD"; exit 2; }
[ -z "$(git -C "$WT" status --porcelain --untracked-files=no -- src include tests bench examples CMakeLists.txt cmake scripts)" ] || { echo "SOURCE_DIRTY"; exit 2; }
mkdir -p "$D/tsan-r2"
bash "$L/tsan-r2.sh" "$D/tsan-r2"
rc=$?
echo "STEP tsan-r2 rc=$rc $(date -Is)"
echo "worst=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
