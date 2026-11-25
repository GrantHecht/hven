#!/usr/bin/env bash
# M8 TR-B (../DECLARATION-r2.md): the r2 legs (ctest rel+dbg, mutations, verdict), STOPPING at the first failed leg (evidence written so far stays).
# Run AFTER ../runtime/run_all.sh (solo, serial).
# usage: chain-r2.sh <code-head sha>      Run as: flock /tmp/box-build.lock bash chain.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/tr-b
D=$WT/docs/notes/data/2026-11-m8-tr-b
L=$D/legs
HEAD_SHA=${1:?code head sha}
BASE_SHA=e43d9f8b5
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export GIT_CEILING_DIRECTORIES=$WT
git -C "$WT" diff --quiet "$HEAD_SHA" -- src include tests bench examples CMakeLists.txt cmake scripts || { echo "SOURCE_MOVED_SINCE_CODE_HEAD"; exit 2; }
[ -z "$(git -C "$WT" status --porcelain --untracked-files=no -- src include tests bench examples CMakeLists.txt cmake scripts)" ] || { echo "SOURCE_DIRTY"; exit 2; }
R=$WT/.scratch
mkdir -p "$R/trb-suites" "$D/suites" "$D/replay" "$D/interior" "$D/install-smoke" "$D/mutation"
step() { local name=$1; shift; "$@"; local rc=$?; echo "STEP $name rc=$rc $(date -Is)"; if [ $rc -ne 0 ]; then echo "CHAIN_STOPPED_AT $name"; echo "FOREGROUND_END $(date -Is)"; exit $rc; fi; }
for b in rel dbg; do
  bash "$L/ctest.sh" $b "$R/trb-suites/ctest-$b.log"; rc=$?
  tail -40 "$R/trb-suites/ctest-$b.log" | grep -E 'tests passed|tests failed|Not Run|\*\*\*' > "$D/suites/ctest-$b-summary.txt"
  step ctest-$b test $rc -eq 0
done
step mutations bash "$L/mutations.sh" "$HEAD_SHA" "$D/mutation"
step mutation-verdict python3 "$L/mutation_verdict.py" "$D/mutation/mutation-table.csv" "$D/mutation/verdict.txt"
echo "CHAIN_COMPLETE"
echo "FOREGROUND_END $(date -Is)"
exit 0
