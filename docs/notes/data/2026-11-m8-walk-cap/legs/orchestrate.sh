#!/usr/bin/env bash
# M8 walk cap (../DECLARATION.md section 5): the legs in order, stopping at the first failure.
#   F1 (rel, dbg), I1, I3, F3, I4, I2.
# Checks first: the code paths of HEAD equal the code commit and are clean; the frozen binaries' sha256.
# usage: orchestrate.sh <code sha> <corpus sha256> <lane sha256>
#   Run as: flock /tmp/box-build.lock bash orchestrate.sh ... > ../orchestrate.out 2>&1
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a7a2ce81cf8988de5
D=$WT/docs/notes/data/2026-11-m8-walk-cap
L=$D/legs
CODE=${1:?code sha}
CS=${2:?corpus sha256}
LS=${3:?lane sha256}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
PATHS="src include tests bench examples bindings CMakeLists.txt cmake scripts"
git -C "$WT" diff --quiet "$CODE" HEAD -- $PATHS || { echo "CODE_PATHS_DIFFER_FROM $CODE"; exit 2; }
[ -z "$(git -C "$WT" status --porcelain --untracked-files=no -- $PATHS)" ] || { echo CODE_PATHS_DIRTY; exit 2; }
echo "code paths == $CODE, clean; HEAD $(git -C "$WT" rev-parse HEAD)"
[ "$(sha256sum "$WT/build-wc/bin/hven_sqp_corpus" | cut -d' ' -f1)" = "$CS" ] || { echo CORPUS_SHA; exit 4; }
[ "$(sha256sum "$WT/build-wc/bin/hven_bench_cell" | cut -d' ' -f1)" = "$LS" ] || { echo LANE_SHA; exit 4; }
mkdir -p "$D/suites" "$D/replay" "$D/interior" "$D/install-smoke" "$D/train" "$D/census"
leg() { local name=$1; shift
  echo "LEG $name start $(date -Is)"
  "$@"; local rc=$?
  echo "LEG $name rc=$rc $(date -Is)"
  if [ $rc -ne 0 ]; then echo "STOP at $name"; echo "FOREGROUND_END $(date -Is)"; exit $rc; fi; }
suites_ok() { grep -q "100% tests passed, 0 tests failed out of 3635" "$1" || { echo "F1_CRITERION_NOT_MET $1"; return 1; }; }
leg F1-rel bash "$L/ctest.sh" rel "$D/suites/ctest-rel.log"
leg F1-rel-criterion suites_ok "$D/suites/ctest-rel.log"
leg F1-dbg bash "$L/ctest.sh" dbg "$D/suites/ctest-dbg.log"
leg F1-dbg-criterion suites_ok "$D/suites/ctest-dbg.log"
leg I1 bash "$L/replay.sh" "$D/replay" "$CS"
leg I3 bash "$L/interior.sh" "$D/interior" "$CS"
leg F3 bash "$L/install_smoke.sh" "$D/install-smoke"
leg I4 bash "$L/train_enum.sh" "$D/train" "$LS" "$D/dev/train" "$L/movers-train.txt"
leg I2 bash "$L/census.sh" "$D/census" "$CS" "$WT/docs/notes/data/2026-11-m8-dn1-impl/census-movers.csv"
echo "ALL_LEGS_PASSED"
echo "FOREGROUND_END $(date -Is)"
