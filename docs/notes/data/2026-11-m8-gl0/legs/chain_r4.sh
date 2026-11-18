#!/usr/bin/env bash
# M8 GL-0 (../DECLARATION-r4.md): Release+Debug build and ctest, corpus sha, replay A vs B, install smoke, mutations.
# usage: chain_r4.sh <code head sha>      Run as: flock /tmp/box-build.lock setsid -w bash chain_r4.sh <sha>
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/gl0
D=$WT/docs/notes/data/2026-11-m8-gl0
L=$D/legs
HEAD_SHA=${1:?code head sha}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export GIT_CEILING_DIRECTORIES=$WT MKL_NUM_THREADS=1
git -C "$WT" diff --quiet "$HEAD_SHA" -- src include tests bench examples CMakeLists.txt cmake scripts || { echo "SOURCE_MOVED_SINCE_CODE_HEAD"; exit 2; }
[ -z "$(git -C "$WT" status --porcelain --untracked-files=no -- src include tests bench examples CMakeLists.txt cmake scripts)" ] || { echo "SOURCE_DIRTY"; exit 2; }
R=$WT/.scratch
O=$D/r4
mkdir -p "$O/suites" "$O/install-smoke" "$O/mutation" "$O/replay"
step() { local name=$1; shift; "$@"; local rc=$?; echo "STEP $name rc=$rc $(date -Is)"; if [ $rc -ne 0 ]; then echo "CHAIN_STOPPED_AT $name"; echo "FOREGROUND_END $(date -Is)"; exit $rc; fi; }
for b in Release:rel Debug:dbg; do
  bash "$L/build.sh" ${b%%:*} ${b##*:} > "$R/r4-build-${b##*:}.log" 2>&1; rc=$?; tail -2 "$R/r4-build-${b##*:}.log"
  step build-${b##*:} test $rc -eq 0
done
mkdir -p "$R/build-gl0/armBbin"
cp -p "$R/build-gl0/rel/bench/hven_sqp_corpus" "$R/build-gl0/armBbin/hven_sqp_corpus" || exit 1
ARMB=$(sha256sum "$R/build-gl0/armBbin/hven_sqp_corpus" | cut -d' ' -f1)
ARMA=$(sha256sum "$R/build-gl0/armAbin/hven_sqp_corpus" | cut -d' ' -f1)
echo "ARMA=$ARMA ARMB=$ARMB"
sha256sum "$R/build-gl0/armAbin/hven_sqp_corpus" "$R/build-gl0/armBbin/hven_sqp_corpus" > "$O/corpus-sha.txt"
for b in rel dbg; do
  bash "$L/ctest.sh" $b "$R/r4-ctest-$b.log"; rc=$?
  grep -E '% tests passed|tests failed' "$R/r4-ctest-$b.log" > "$O/suites/ctest-$b-summary.txt"
  step ctest-$b test $rc -eq 0
done
step replay bash "$L/replay.sh" "$R/r4-replay" "$ARMB" "$ARMA"
cp "$R"/r4-replay/gl0-comparison-*.txt "$O/replay/" 2>/dev/null
bash "$L/install_smoke.sh" "$R/r4-install-smoke"; rc=$?
for a in default clang; do tail -4 "$R/r4-install-smoke/smoke-$a.log" > "$O/install-smoke/smoke-$a-tail.txt" 2>/dev/null; done
grep -hE 'export contract OK|FAIL|rc=' "$R"/r4-install-smoke/export-contract-*.log > "$O/install-smoke/export-contract-summary.txt" 2>/dev/null
step install-smoke test $rc -eq 0
RAW_DIR=$R/r4-mutation step mutations bash "$L/mutations.sh" "$HEAD_SHA" "$O/mutation"
MUST_EXTRA="n1 n1b n2" step mutation-verdict python3 "$L/mutation_verdict.py" "$O/mutation/mutation-table.csv" "$O/mutation/verdict.txt"
echo "CHAIN_COMPLETE"
echo "FOREGROUND_END $(date -Is)"
