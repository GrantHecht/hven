#!/usr/bin/env bash
# M8 AP-5 G1 (../DECLARATION.txt): the non-timing legs in sequence, after the runtime gate (run first and alone by
# ../runtime/run_all.sh). Shape from ../../2026-11-m8-dn3b/legs/chain.sh. P1 (suites, the golden rig inside), P3
# (replay), P4 (interior), P5 (install smoke + export contract), P8 (ThreadSanitizer), in that order; every step runs
# and the worst rc is the chain's.
# usage: chain.sh <code-head sha> <expected arm B sha256>   Run as: flock /tmp/box-build.lock bash chain.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-afb4015ceb50aa1d1
D=$WT/docs/notes/data/2026-11-m8-ap5-g1
L=$D/legs
HEAD_SHA=${1:?code head sha}
ARMB=${2:?arm B sha256}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
git -C "$WT" diff --quiet "$HEAD_SHA" -- src include tests bench examples CMakeLists.txt cmake scripts || { echo "SOURCE_MOVED_SINCE_CODE_HEAD"; exit 2; }
[ -z "$(git -C "$WT" status --porcelain --untracked-files=no -- src include tests bench examples CMakeLists.txt cmake scripts)" ] || { echo "SOURCE_DIRTY"; exit 2; }
[ -f "$WT/scripts/compare_replay.py" ] || { echo "COMPARE_NOT_FOUND"; exit 7; }
mkdir -p "$D/suites" "$D/replay" "$D/interior" "$D/install-smoke" "$D/tsan"
worst=0
step() { local name=$1; shift; "$@"; local rc=$?; echo "STEP $name rc=$rc $(date -Is)"; [ $rc -gt $worst ] && worst=$rc; }
step ctest-rel bash "$L/ctest.sh" rel "$D/suites/ctest-rel.log"
step ctest-dbg bash "$L/ctest.sh" dbg "$D/suites/ctest-dbg.log"
step golden-rig-listing grep -E "hven_golden_rig|GoldenRig|Rig" "$D/suites/ctest-rel.log"
step replay bash "$L/replay.sh" "$D/replay" "$ARMB"
step interior bash "$L/interior.sh" "$D/interior" "$ARMB"
step install-smoke bash "$L/install_smoke.sh" "$D/install-smoke"
step tsan bash "$L/tsan.sh" "$D/tsan"
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
