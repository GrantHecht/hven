#!/usr/bin/env bash
# M8 dense gate (../DECLARATION-r2.md): the legs in order -- F1 (rel, dbg), I1, I3, F3, I4, I2. Stops at the first failed
# leg. Every path absolute.
# usage: orchestrate.sh <code head sha> <corpus binary sha256> <lane binary sha256> <F1 count>
# Run as: flock /tmp/box-build.lock bash orchestrate.sh ... > ../legs-out/orchestrate.out 2>&1
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a44d4824b0beb0ea7
DG=$WT/docs/notes/data/2026-11-m8-dense-gate/r2
L=$DG/legs
O=$DG/legs-out
HEADSHA=${1:?code head sha}
CORPUS=${2:?corpus binary sha256}
LANE=${3:?lane binary sha256}
COUNT=${4:?F1 test count}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
git -C "$WT" diff --quiet "$HEADSHA" -- src include tests bench examples CMakeLists.txt cmake scripts \
  || { echo "SOURCE_MOVED_SINCE_CODE_HEAD"; exit 2; }
[ -z "$(git -C "$WT" status --porcelain --untracked-files=no -- src include tests bench examples CMakeLists.txt cmake scripts)" ] \
  || { echo "SOURCE_DIRTY"; exit 2; }
export TMPDIR=$WT/build-dg/tmp
mkdir -p "$TMPDIR" "$O/suites"
leg() { local name=$1; shift; echo "LEG $name start $(date -Is)"; "$@"; local rc=$?; echo "LEG $name rc=$rc $(date -Is)";
        [ $rc -eq 0 ] || { echo "STOPPED_AT $name"; echo "FOREGROUND_END $(date -Is)"; exit $rc; }; }
leg F1-rel bash "$L/ctest.sh" rel "$O/suites/ctest-rel.log"
grep -q "100% tests passed, 0 tests failed out of $COUNT" "$O/suites/ctest-rel.log" || { echo "F1-rel COUNT_MISMATCH"; echo "STOPPED_AT F1-rel"; exit 1; }
leg F1-dbg bash "$L/ctest.sh" dbg "$O/suites/ctest-dbg.log"
grep -q "100% tests passed, 0 tests failed out of $COUNT" "$O/suites/ctest-dbg.log" || { echo "F1-dbg COUNT_MISMATCH"; echo "STOPPED_AT F1-dbg"; exit 1; }
leg I1 bash "$L/replay.sh" "$O/replay" "$CORPUS"
leg I3 bash "$L/interior.sh" "$O/interior" "$CORPUS"
leg F3 bash "$L/install_smoke.sh" "$O/install-smoke"
leg I4 bash "$DG/sl/enum.sh" "$O/identity" "$LANE" "$DG/train-movers.txt" "$DG/dev/identity"
leg I2 bash "$L/census.sh" "$O/census" "$CORPUS"
echo "ALL_LEGS_PASSED"
echo "FOREGROUND_END $(date -Is)"
