#!/usr/bin/env bash
# worst rc is the chain's. Run AFTER ../runtime/run_all.sh and ../ic/run_ic.sh (those are solo).
# usage: chain.sh <code-head sha> <expected arm B sha256> <expected arm A sha256>      Run as: flock /tmp/box-build.lock bash chain.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/g6
D=$WT/docs/notes/data/2026-11-m8-ap5-g6
L=$D/legs
HEAD_SHA=${1:?code head sha}
BASE_SHA=7c70be63a
ARMB=${2:?arm B sha256}
ARMA=${3:?arm A sha256}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
git -C "$WT" diff --quiet "$HEAD_SHA" -- src include tests bench examples CMakeLists.txt cmake scripts || { echo "SOURCE_MOVED_SINCE_CODE_HEAD"; exit 2; }
[ -z "$(git -C "$WT" status --porcelain --untracked-files=no -- src include tests bench examples CMakeLists.txt cmake scripts)" ] || { echo "SOURCE_DIRTY"; exit 2; }
mkdir -p "$WT/.scratch/g6-suites" "$D/suites" "$D/replay" "$D/interior" "$D/install-smoke" "$D/mutation"
worst=0
# G5: STOPS at the first failed step (the evidence written so far stays).
step() { local name=$1; shift; "$@"; local rc=$?; echo "STEP $name rc=$rc $(date -Is)"; if [ $rc -ne 0 ]; then echo "CHAIN_STOPPED_AT $name"; echo "FOREGROUND_END $(date -Is)"; exit $rc; fi; }
for b in rel dbg; do
  bash "$L/ctest.sh" $b "$WT/.scratch/g6-suites/ctest-$b.log"; rc=$?
  tail -40 "$WT/.scratch/g6-suites/ctest-$b.log" | grep -E 'tests passed|tests failed|Not Run|\*\*\*' > "$D/suites-r3/ctest-$b-summary.txt"
  step ctest-$b test $rc -eq 0
done
step replay bash "$L/replay.sh" "$D/replay" "$ARMB" "$ARMA"
step interior bash "$L/interior.sh" "$D/interior" "$ARMB"
python3 "$L/count_multiphase_restoration.py" "$D/interior/g6-interior-B-baseline.csv" > "$D/interior/multiphase-restoration-count-B.txt"; cat "$D/interior/multiphase-restoration-count-B.txt"
bash "$L/install_smoke.sh" "$WT/.scratch/g6-install-smoke"; rc=$?
grep -hE 'export contract OK|FAIL|rc=' "$WT"/.scratch/g6-install-smoke/export-contract-*.log > "$D/install-smoke-r3/export-contract-summary.txt" 2>/dev/null
for a in default clang; do tail -4 "$WT/.scratch/g6-install-smoke/smoke-$a.log" > "$D/install-smoke-r3/smoke-$a-tail.txt" 2>/dev/null; done
step install-smoke test $rc -eq 0
git -C "$WT" diff --quiet "$HEAD_SHA" -- . ':!docs' || true
python3 "$L/goldens_rename_check.py" "$BASE_SHA" "$HEAD_SHA" "$D/goldens-rename-check.txt"; rc=$?
step goldens-rename test $rc -eq 0
step mutations bash "$L/mutations.sh" "$HEAD_SHA" "$D/mutation"
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
