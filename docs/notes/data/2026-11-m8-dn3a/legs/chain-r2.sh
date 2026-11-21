#!/usr/bin/env bash
# M8 DN-3a r2 (../DECLARATION-r2.txt): legs/chain.sh re-pointed for r2 (outputs suites-r2/, replay-r2/, interior-r2/,
# failproof-r2/; the falsifier legs/failproof-r2.sh). P1, P2, P3, P4 in that order.
# usage: chain-r2.sh <code-head sha> <expected arm B sha256>   Run as: flock /tmp/box-build.lock bash chain-r2.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-acfc8f7d568381148
D=$WT/docs/notes/data/2026-11-m8-dn3a
L=$D/legs
HEAD_SHA=${1:?code head sha}
ARMB=${2:?arm B sha256}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
git -C "$WT" diff --quiet "$HEAD_SHA" -- src include tests bench CMakeLists.txt cmake || { echo "SOURCE_MOVED_SINCE_CODE_HEAD"; exit 2; }
[ -f "$WT/scripts/compare_replay.py" ] || { echo "COMPARE_NOT_FOUND"; exit 7; }
mkdir -p "$D/suites-r2" "$D/replay-r2" "$D/interior-r2" "$D/failproof-r2"
worst=0
step() { local name=$1; shift; "$@"; local rc=$?; echo "STEP $name rc=$rc $(date -Is)"; [ $rc -gt $worst ] && worst=$rc; }
step ctest-rel bash "$L/ctest.sh" rel "$D/suites-r2/ctest-rel.log"
step ctest-dbg bash "$L/ctest.sh" dbg "$D/suites-r2/ctest-dbg.log"
step replay bash "$L/replay.sh" "$D/replay-r2"
step interior bash "$L/interior.sh" "$D/interior-r2" "$ARMB"
step failproof bash "$L/failproof-r2.sh" "$HEAD_SHA" "$D/failproof-r2"
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
