#!/usr/bin/env bash
# M8 AP-5 G6 r2 (../DECLARATION-r2.md): the r1 chain's ctest and mutation steps only (replay, interior, install smoke and the goldens check are carried on a
# source-identity proof, see the declaration). Stops at the first failed step.
# usage: chain-r2.sh <code-head sha> <r1 code head sha>      Run as: flock /tmp/box-build.lock bash chain-r2.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/g6
D=$WT/docs/notes/data/2026-11-m8-ap5-g6
L=$D/legs
HEAD_SHA=${1:?code head sha}
R1=${2:?r1 code head sha}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
git -C "$WT" diff --quiet "$HEAD_SHA" -- src include tests bench examples CMakeLists.txt cmake scripts || { echo "SOURCE_MOVED_SINCE_CODE_HEAD"; exit 2; }
[ -z "$(git -C "$WT" status --porcelain --untracked-files=no -- src include tests bench examples CMakeLists.txt cmake scripts)" ] || { echo "SOURCE_DIRTY"; exit 2; }
# the carried legs' premise, re-proved at the start: src/include/bench/examples/cmake/scripts are byte-identical to the r1 code head
git -C "$WT" diff --quiet "$R1" "$HEAD_SHA" -- src include bench examples CMakeLists.txt cmake scripts || { echo "CARRY_PREMISE_BROKEN"; exit 2; }
git -C "$WT" diff --stat "$R1" "$HEAD_SHA" -- src include bench tests > "$D/carry-premise-r2.txt"
mkdir -p "$WT/.scratch/g6r2-suites" "$D/suites-r2" "$D/mutation-r2"
step() { local name=$1; shift; "$@"; local rc=$?; echo "STEP $name rc=$rc $(date -Is)"; if [ $rc -ne 0 ]; then echo "CHAIN_STOPPED_AT $name"; echo "FOREGROUND_END $(date -Is)"; exit $rc; fi; }
for b in rel dbg; do
  bash "$L/ctest.sh" $b "$WT/.scratch/g6r2-suites/ctest-$b.log"; rc=$?
  tail -n 40 "$WT/.scratch/g6r2-suites/ctest-$b.log" | grep -E 'tests passed|tests failed|Not Run|\*\*\*' > "$D/suites-r2/ctest-$b-summary.txt"
  step ctest-$b test $rc -eq 0
done
export RAW_DIR=$WT/.scratch/g6r2-mutation MUT_IDS="c3 c4 c11 c12 p1"
step mutations bash "$L/mutations.sh" "$HEAD_SHA" "$D/mutation-r2"
echo "FOREGROUND_END $(date -Is)"
