#!/usr/bin/env bash
# M8 AP-4b (../DECLARATION-AP4b.md): the proof legs for one group head, STOPPING at the first failed leg.
# Requires build-ap4b/rel and build-ap4b/dbg built at <head> (legs/build.sh) and build-ap4b/armAbin from legs/prebuild.sh.
# usage: chain.sh <tag> <head sha>      Run as: box-lane -- bash chain.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/ap4b
D=$WT/docs/notes/data/2026-11-m8-ap4b
L=$D/legs
TAG=${1:?group tag}
HEAD_SHA=${2:?code head sha}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; exit 3; fi
export GIT_CEILING_DIRECTORIES=$WT
git -C "$WT" diff --quiet "$HEAD_SHA" -- src include tests bench examples CMakeLists.txt cmake scripts || { echo "SOURCE_MOVED_SINCE_HEAD"; exit 2; }
R=$WT/.scratch/$TAG; E=$D/$TAG
mkdir -p "$R" "$E"
step() { local name=$1; shift; "$@"; local rc=$?; echo "STEP $name rc=$rc $(date -Is)"; if [ $rc -ne 0 ]; then echo "CHAIN_STOPPED_AT $name"; exit $rc; fi; }
for b in rel dbg; do
  bash "$L/ctest.sh" $b "$R/ctest-$b.log"; rc=$?
  grep -E 'tests passed|tests failed|Not Run|\*\*\*' "$R/ctest-$b.log" | tail -5 > "$E/ctest-$b-summary.txt"
  step ctest-$b test $rc -eq 0
done
mkdir -p "$WT/build-ap4b/armBbin"
cp -p "$WT/build-ap4b/rel/bench/hven_sqp_corpus" "$WT/build-ap4b/armBbin/hven_sqp_corpus"
ARMB=$(sha256sum "$WT/build-ap4b/armBbin/hven_sqp_corpus" | cut -d' ' -f1)
ARMA=$(sha256sum "$WT/build-ap4b/armAbin/hven_sqp_corpus" | cut -d' ' -f1)
echo "armA $ARMA armB $ARMB" | tee "$E/binaries.txt"
step replay bash "$L/replay.sh" "$R/replay" "$ARMB" "$ARMA"
mkdir -p "$E/replay"; cp "$R"/replay/ap4b-comparison-*.txt "$E/replay/"
grep -H "differences" "$E"/replay/ap4b-comparison-*.txt > "$E/replay-summary.txt"
bash "$L/install_smoke.sh" "$R/smoke"; rc=$?
grep -hE 'export contract OK|FAIL|rc=' "$R"/smoke/export-contract-*.log > "$E/export-contract-summary.txt" 2>/dev/null
for a in default clang; do tail -4 "$R/smoke/smoke-$a.log" > "$E/smoke-$a-tail.txt" 2>/dev/null; done
step install-smoke test $rc -eq 0
echo "CHAIN_COMPLETE $TAG $HEAD_SHA"
