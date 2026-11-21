#!/usr/bin/env bash
# M8 GL-0 (../DECLARATION-r3.md): rebased head: Release+Debug build and ctest, install smoke, corpus sha, mutations.
# usage: chain_r3.sh <code head sha>      Run as: flock /tmp/box-build.lock setsid -w bash chain_r3.sh <sha>
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/gl0
D=$WT/docs/notes/data/2026-11-m8-gl0
L=$D/legs
HEAD_SHA=${1:?code head sha}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export GIT_CEILING_DIRECTORIES=$WT
git -C "$WT" diff --quiet "$HEAD_SHA" -- src include tests bench examples CMakeLists.txt cmake scripts || { echo "SOURCE_MOVED_SINCE_CODE_HEAD"; exit 2; }
[ -z "$(git -C "$WT" status --porcelain --untracked-files=no -- src include tests bench examples CMakeLists.txt cmake scripts)" ] || { echo "SOURCE_DIRTY"; exit 2; }
R=$WT/.scratch
mkdir -p "$D/r3/suites" "$D/r3/install-smoke" "$D/r3/mutation"
step() { local name=$1; shift; "$@"; local rc=$?; echo "STEP $name rc=$rc $(date -Is)"; if [ $rc -ne 0 ]; then echo "CHAIN_STOPPED_AT $name"; echo "FOREGROUND_END $(date -Is)"; exit $rc; fi; }
for b in Release:rel Debug:dbg; do
  bash "$L/build.sh" ${b%%:*} ${b##*:} > "$R/r3-build-${b##*:}.log" 2>&1; rc=$?; tail -2 "$R/r3-build-${b##*:}.log"
  step build-${b##*:} test $rc -eq 0
done
sha256sum "$R/build-gl0/rel/bench/hven_sqp_corpus" > "$D/r3/corpus-sha-rebased.txt"
sha256sum "$R/build-gl0/armBbin/hven_sqp_corpus" >> "$D/r3/corpus-sha-rebased.txt"
for b in rel dbg; do
  bash "$L/ctest.sh" $b "$R/r3-ctest-$b.log"; rc=$?
  grep -E '% tests passed|tests failed' "$R/r3-ctest-$b.log" > "$D/r3/suites/ctest-$b-summary.txt"
  step ctest-$b test $rc -eq 0
done
bash "$L/install_smoke.sh" "$R/gl0-install-smoke-r3"; rc=$?
for a in default clang; do tail -4 "$R/gl0-install-smoke-r3/smoke-$a.log" > "$D/r3/install-smoke/smoke-$a-tail.txt" 2>/dev/null; done
grep -hE 'export contract OK|FAIL|rc=' "$R"/gl0-install-smoke-r3/export-contract-*.log > "$D/r3/install-smoke/export-contract-summary.txt" 2>/dev/null
step install-smoke test $rc -eq 0
step mutations bash "$L/mutations.sh" "$HEAD_SHA" "$D/r3/mutation"
step mutation-verdict python3 "$L/mutation_verdict.py" "$D/r3/mutation/mutation-table.csv" "$D/r3/mutation/verdict.txt"
echo "CHAIN_COMPLETE"
echo "FOREGROUND_END $(date -Is)"
