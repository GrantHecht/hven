#!/usr/bin/env bash
# M8 GL-0 (../DECLARATION.md): the non-timing legs in order, STOPPING at the first failed leg (evidence written so far stays).
# Run AFTER ../runtime/run_all.sh (solo, serial).
# usage: chain.sh <code-head sha> <expected arm B sha256> <expected arm A sha256>      Run as: flock /tmp/box-build.lock bash chain.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/gl0
D=$WT/docs/notes/data/2026-11-m8-gl0
L=$D/legs
HEAD_SHA=${1:?code head sha}
BASE_SHA=3edf5ede4
ARMB=${2:?arm B sha256}
ARMA=${3:?arm A sha256}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export GIT_CEILING_DIRECTORIES=$WT
git -C "$WT" diff --quiet "$HEAD_SHA" -- src include tests bench examples CMakeLists.txt cmake scripts || { echo "SOURCE_MOVED_SINCE_CODE_HEAD"; exit 2; }
[ -z "$(git -C "$WT" status --porcelain --untracked-files=no -- src include tests bench examples CMakeLists.txt cmake scripts)" ] || { echo "SOURCE_DIRTY"; exit 2; }
R=$WT/.scratch
mkdir -p "$R/gl0-suites" "$D/suites" "$D/replay" "$D/interior" "$D/install-smoke" "$D/mutation"
step() { local name=$1; shift; "$@"; local rc=$?; echo "STEP $name rc=$rc $(date -Is)"; if [ $rc -ne 0 ]; then echo "CHAIN_STOPPED_AT $name"; echo "FOREGROUND_END $(date -Is)"; exit $rc; fi; }
for b in rel dbg; do
  bash "$L/ctest.sh" $b "$R/gl0-suites/ctest-$b.log"; rc=$?
  tail -40 "$R/gl0-suites/ctest-$b.log" | grep -E 'tests passed|tests failed|Not Run|\*\*\*' > "$D/suites/ctest-$b-summary.txt"
  step ctest-$b test $rc -eq 0
done
step replay bash "$L/replay.sh" "$R/gl0-replay" "$ARMB" "$ARMA"
mkdir -p "$D/replay"; cp "$R"/gl0-replay/gl0-comparison-*.txt "$R"/gl0-replay/*.csv "$D/replay/" 2>/dev/null
step interior bash "$L/interior.sh" "$R/gl0-interior" "$ARMB"
cp "$R"/gl0-interior/gl0-interior-comparison-*.txt "$R"/gl0-interior/*.csv "$D/interior/" 2>/dev/null
bash "$L/install_smoke.sh" "$R/gl0-install-smoke"; rc=$?
grep -hE 'export contract OK|FAIL|rc=' "$R"/gl0-install-smoke/export-contract-*.log > "$D/install-smoke/export-contract-summary.txt" 2>/dev/null
for a in default clang; do tail -4 "$R/gl0-install-smoke/smoke-$a.log" > "$D/install-smoke/smoke-$a-tail.txt" 2>/dev/null; done
step install-smoke test $rc -eq 0
step mutations bash "$L/mutations.sh" "$HEAD_SHA" "$D/mutation"
step mutation-verdict python3 "$L/mutation_verdict.py" "$D/mutation/mutation-table.csv" "$D/mutation/verdict.txt"
echo "CHAIN_COMPLETE"
echo "FOREGROUND_END $(date -Is)"
exit 0
