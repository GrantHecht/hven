#!/usr/bin/env bash
# M8 AP-5 G2 r3 (../DECLARATION-r3.md): P1 ctest Release + Debug, P3 replay + ARMED, P4 interior, P8 TSan, then the mutation table.
# Every step runs; the worst rc is the chain's. usage: chain.sh <code-head sha> <expected arm B3 sha256>
# Run as: flock /tmp/box-build.lock bash chain.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a41fdd4ad89efc301
D=$WT/docs/notes/data/2026-11-m8-ap5-g2
L=$D/legs-r3
HEAD_SHA=${1:?code head sha}
ARMB=${2:?arm B3 sha256}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
git -C "$WT" diff --quiet "$HEAD_SHA" -- src include tests bench examples CMakeLists.txt cmake scripts || { echo "SOURCE_MOVED_SINCE_CODE_HEAD"; exit 2; }
[ -z "$(git -C "$WT" status --porcelain --untracked-files=no -- src include tests bench examples CMakeLists.txt cmake scripts)" ] || { echo "SOURCE_DIRTY"; exit 2; }
mkdir -p "$D/suites-r3" "$D/replay-r3" "$D/interior-r3" "$D/tsan-r3" "$D/mutation-r3"
worst=0
step() { local name=$1; shift; "$@"; local rc=$?; echo "STEP $name rc=$rc $(date -Is)"; [ $rc -gt $worst ] && worst=$rc; }
step ctest-rel bash "$L/ctest.sh" rel "$D/suites-r3/ctest-rel.log"
step ctest-dbg bash "$L/ctest.sh" dbg "$D/suites-r3/ctest-dbg.log"
step replay bash "$L/replay.sh" "$D/replay-r3" "$ARMB"
step interior bash "$L/interior.sh" "$D/interior-r3" "$ARMB"
step tsan bash "$L/tsan.sh" "$D/tsan-r3"
step mutations bash "$L/mutations.sh" "$HEAD_SHA" "$D/mutation-r3"
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
