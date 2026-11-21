#!/usr/bin/env bash
# M8 DN-3 r4 (../../DECLARATION-r4.md): the r4 legs in declared order, one after another, each its own script. Stops at
# the first failed leg (ON FAIL: stop, commit the evidence, report). Outputs under ../ (r4/): suites/, replay/, iparm/,
# interior/, install-smoke/, identity/, census/, and this script's log orchestrate.out (written by the caller's redirect).
# usage: orchestrate-r4.sh <flip sha> <corpus binary sha256> <lane binary sha256>
# Run as: flock /tmp/box-build.lock bash orchestrate-r4.sh ... > ../orchestrate.out 2>&1
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a739270f237b3e30c
R4=$WT/docs/notes/data/2026-11-m8-dn3/r4
L=$R4/legs
FLIP=${1:?flip sha}
CORPUS=${2:?corpus binary sha256}
LANE=${3:?lane binary sha256}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
git -C "$WT" diff --quiet "$FLIP" -- src include tests bench examples CMakeLists.txt cmake scripts \
  || { echo "SOURCE_MOVED_SINCE_FLIP_COMMIT"; exit 2; }
[ -z "$(git -C "$WT" status --porcelain --untracked-files=no -- src include tests bench examples CMakeLists.txt cmake scripts)" ] \
  || { echo "SOURCE_DIRTY"; exit 2; }
export TMPDIR=$WT/build-flip/tmp
mkdir -p "$TMPDIR" "$R4/suites"
leg() { local name=$1; shift; echo "LEG $name start $(date -Is)"; "$@"; local rc=$?; echo "LEG $name rc=$rc $(date -Is)";
        [ $rc -eq 0 ] || { echo "STOPPED_AT $name"; echo "FOREGROUND_END $(date -Is)"; exit $rc; }; }
leg F1-rel bash "$L/ctest.sh" rel "$R4/suites/ctest-rel.log"
leg F1-dbg bash "$L/ctest.sh" dbg "$R4/suites/ctest-dbg.log"
leg I1 bash "$L/replay.sh" "$R4/replay" "$CORPUS"
leg I3ab bash "$L/iparm.sh" "$R4/iparm"
leg I3c bash "$L/interior.sh" "$R4/interior" "$CORPUS"
leg F3 bash "$L/install_smoke.sh" "$R4/install-smoke"
leg I4 bash "$R4/sl/flip_enum.sh" "$R4/identity" "$LANE"
leg I2 bash "$L/census.sh" "$R4/census" "$CORPUS"
echo "ALL_LEGS_PASSED"
echo "FOREGROUND_END $(date -Is)"
