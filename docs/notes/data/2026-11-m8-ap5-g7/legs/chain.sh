#!/usr/bin/env bash
# M8 AP-5 G7 (../DECLARATION.md): the non-timing legs in order, STOPPING at the first failed leg (evidence written so far stays).
# Run AFTER ../runtime/run_all.sh (solo, serial).
# usage: chain.sh <code-head sha> <expected arm B sha256> <expected arm A sha256>      Run as: flock /tmp/box-build.lock bash chain.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/g7
D=$WT/docs/notes/data/2026-11-m8-ap5-g7
L=$D/legs
HEAD_SHA=${1:?code head sha}
BASE_SHA=1dacda16f
ARMB=${2:?arm B sha256}
ARMA=${3:?arm A sha256}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export GIT_CEILING_DIRECTORIES=$WT
git -C "$WT" diff --quiet "$HEAD_SHA" -- src include tests bench examples CMakeLists.txt cmake scripts || { echo "SOURCE_MOVED_SINCE_CODE_HEAD"; exit 2; }
[ -z "$(git -C "$WT" status --porcelain --untracked-files=no -- src include tests bench examples CMakeLists.txt cmake scripts)" ] || { echo "SOURCE_DIRTY"; exit 2; }
R=$WT/.scratch
mkdir -p "$R/g7-suites" "$D/suites" "$D/replay" "$D/interior" "$D/install-smoke" "$D/mutation" "$D/tsan"
step() { local name=$1; shift; "$@"; local rc=$?; echo "STEP $name rc=$rc $(date -Is)"; if [ $rc -ne 0 ]; then echo "CHAIN_STOPPED_AT $name"; echo "FOREGROUND_END $(date -Is)"; exit $rc; fi; }
for b in rel dbg; do
  bash "$L/ctest.sh" $b "$R/g7-suites/ctest-$b.log"; rc=$?
  tail -40 "$R/g7-suites/ctest-$b.log" | grep -E 'tests passed|tests failed|Not Run|\*\*\*' > "$D/suites/ctest-$b-summary.txt"
  step ctest-$b test $rc -eq 0
done
step replay bash "$L/replay.sh" "$R/g7-replay" "$ARMB" "$ARMA"
mkdir -p "$D/replay"; cp "$R"/g7-replay/g7-comparison-*.txt "$R"/g7-replay/*.csv "$D/replay/" 2>/dev/null
step interior bash "$L/interior.sh" "$R/g7-interior" "$ARMB"
cp "$R"/g7-interior/g7-interior-comparison-*.txt "$R"/g7-interior/*.csv "$D/interior/" 2>/dev/null
bash "$L/install_smoke.sh" "$R/g7-install-smoke"; rc=$?
grep -hE 'export contract OK|FAIL|rc=' "$R"/g7-install-smoke/export-contract-*.log > "$D/install-smoke/export-contract-summary.txt" 2>/dev/null
for a in default clang; do tail -4 "$R/g7-install-smoke/smoke-$a.log" > "$D/install-smoke/smoke-$a-tail.txt" 2>/dev/null; done
step install-smoke test $rc -eq 0
step goldens python3 "$L/goldens_check.py" "$BASE_SHA" "$HEAD_SHA" "$D/goldens-check.txt"
step tsan bash "$L/tsan.sh" "$D/tsan" "$R/g7-tsan"
step mutations bash "$L/mutations.sh" "$HEAD_SHA" "$D/mutation"
step mutation-verdict python3 "$L/mutation_verdict.py" "$D/mutation/mutation-table.csv" "$D/mutation/verdict.txt"
echo "CHAIN_COMPLETE"
echo "FOREGROUND_END $(date -Is)"
exit 0
