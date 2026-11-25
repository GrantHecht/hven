#!/usr/bin/env bash
# M8 IPM ladder status (../DECLARATION.txt): P1, P2, P3, P4 in that order, each output under this directory.
# Modelled on ../../2026-11-m8-nan-gates/legs/chain.sh (re-pointed; no pch leg: no include list changes).
# usage: chain.sh <code-head sha> <expected arm B sha256>   Run as: flock /tmp/box-build.lock bash chain.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/ipmlad
D=$WT/docs/notes/data/2026-11-m8-ipm-ladder-status
L=$D/legs
HEAD_SHA=${1:?code head sha}
ARMB=${2:?arm B sha256}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
git -C "$WT" diff --quiet "$HEAD_SHA" -- src include tests bench CMakeLists.txt cmake || { echo "SOURCE_MOVED_SINCE_CODE_HEAD"; exit 2; }
[ -f "$WT/scripts/compare_replay.py" ] || { echo "COMPARE_NOT_FOUND"; exit 7; }
mkdir -p "$D/suites" "$D/replay" "$D/interior" "$D/failproof"
worst=0
step() { local name=$1; shift; "$@"; local rc=$?; echo "STEP $name rc=$rc $(date -Is)"; [ $rc -gt $worst ] && worst=$rc; }
step ctest-rel bash "$L/ctest.sh" rel "$D/suites/ctest-rel.log"
step ctest-dbg bash "$L/ctest.sh" dbg "$D/suites/ctest-dbg.log"
step replay bash "$L/replay.sh" "$D/replay"
step interior bash "$L/interior.sh" "$D/interior" "$ARMB"
step failproof bash "$L/failproof.sh" "$HEAD_SHA" "$D/failproof"
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
