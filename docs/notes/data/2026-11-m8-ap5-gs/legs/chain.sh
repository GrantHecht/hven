#!/usr/bin/env bash
# M8 AP-5 G-S (../DECLARATION.md): the legs in order, STOPPING at the first failed leg (evidence written so far stays).
# No runtime leg (declared: no executed default-path code per iteration changes).
# usage: chain.sh <code-head sha> <expected arm B sha256> <expected arm A sha256>      Run as: flock /tmp/box-build.lock bash chain.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/g-rb
D=$WT/docs/notes/data/2026-11-m8-ap5-gs
L=$D/legs
HEAD_SHA=${1:?code head sha}
BASE_SHA=8e6e07310
ARMB=${2:?arm B sha256}
ARMA=${3:?arm A sha256}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export GIT_CEILING_DIRECTORIES=$WT
git -C "$WT" diff --quiet "$HEAD_SHA" -- src include tests bench examples CMakeLists.txt cmake scripts || { echo "SOURCE_MOVED_SINCE_CODE_HEAD"; exit 2; }
[ -z "$(git -C "$WT" status --porcelain --untracked-files=no -- src include tests bench examples CMakeLists.txt cmake scripts)" ] || { echo "SOURCE_DIRTY"; exit 2; }
R=$WT/.scratch
mkdir -p "$R/gs-suites" "$D/suites" "$D/replay" "$D/interior" "$D/install-smoke" "$D/mutation"
step() { local name=$1; shift; "$@"; local rc=$?; echo "STEP $name rc=$rc $(date -Is)"; if [ $rc -ne 0 ]; then echo "CHAIN_STOPPED_AT $name"; echo "FOREGROUND_END $(date -Is)"; exit $rc; fi; }
for b in rel dbg; do
  bash "$L/ctest.sh" $b "$R/gs-suites/ctest-$b.log"; rc=$?
  tail -40 "$R/gs-suites/ctest-$b.log" | grep -E 'tests passed|tests failed|Not Run|\*\*\*' > "$D/suites/ctest-$b-summary.txt"
  step ctest-$b test $rc -eq 0
done
step replay bash "$L/replay.sh" "$R/gs-replay" "$ARMB" "$ARMA"
mkdir -p "$D/replay"; cp "$R"/gs-replay/gs-comparison-*.txt "$R"/gs-replay/*.csv "$D/replay/" 2>/dev/null
step interior bash "$L/interior.sh" "$R/gs-interior" "$ARMB"
cp "$R"/gs-interior/gs-interior-comparison-*.txt "$R"/gs-interior/*.csv "$D/interior/" 2>/dev/null
bash "$L/install_smoke.sh" "$R/gs-install-smoke"; rc=$?
grep -hE 'export contract OK|FAIL|rc=' "$R"/gs-install-smoke/export-contract-*.log > "$D/install-smoke/export-contract-summary.txt" 2>/dev/null
for a in default clang; do tail -4 "$R/gs-install-smoke/smoke-$a.log" > "$D/install-smoke/smoke-$a-tail.txt" 2>/dev/null; done
step install-smoke test $rc -eq 0
step mutations bash "$L/mutations.sh" "$HEAD_SHA" "$D/mutation"
step mutation-verdict python3 "$L/mutation_verdict.py" "$D/mutation/mutation-table.csv" "$D/mutation/verdict.txt"
echo "CHAIN_COMPLETE"
echo "FOREGROUND_END $(date -Is)"
exit 0
