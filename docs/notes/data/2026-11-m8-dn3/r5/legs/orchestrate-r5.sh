#!/usr/bin/env bash
# M8 DN-3 r5 (../../DECLARATION-r5.md): ../../r4/legs/orchestrate-r4.sh re-pointed: the r5 code head, outputs under
# ../ (r5/), the interior leg replaced by ./interior.sh (r5's I3c). Every other leg is r4's instrument, unchanged, by
# path. Stops at the first failed leg.
# usage: orchestrate-r5.sh <code head sha> <corpus binary sha256> <lane binary sha256>
# Run as: flock /tmp/box-build.lock bash orchestrate-r5.sh ... > ../orchestrate.out 2>&1
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a739270f237b3e30c
D3=$WT/docs/notes/data/2026-11-m8-dn3
R5=$D3/r5
L4=$D3/r4/legs
HEADSHA=${1:?code head sha}
CORPUS=${2:?corpus binary sha256}
LANE=${3:?lane binary sha256}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
git -C "$WT" diff --quiet "$HEADSHA" -- src include tests bench examples CMakeLists.txt cmake scripts \
  || { echo "SOURCE_MOVED_SINCE_CODE_HEAD"; exit 2; }
[ -z "$(git -C "$WT" status --porcelain --untracked-files=no -- src include tests bench examples CMakeLists.txt cmake scripts)" ] \
  || { echo "SOURCE_DIRTY"; exit 2; }
git -C "$WT" diff --quiet 2d6d89cd076c64e6a7fa859040fe0b3269a2adf3 "$HEADSHA" -- src include \
  || { echo "LIBRARY_SOURCE_MOVED_SINCE_THE_LANE_PIN"; exit 2; }
export TMPDIR=$WT/build-flip/tmp
mkdir -p "$TMPDIR" "$R5/suites"
leg() { local name=$1; shift; echo "LEG $name start $(date -Is)"; "$@"; local rc=$?; echo "LEG $name rc=$rc $(date -Is)";
        [ $rc -eq 0 ] || { echo "STOPPED_AT $name"; echo "FOREGROUND_END $(date -Is)"; exit $rc; }; }
leg F1-rel bash "$L4/ctest.sh" rel "$R5/suites/ctest-rel.log"
leg F1-dbg bash "$L4/ctest.sh" dbg "$R5/suites/ctest-dbg.log"
leg I1 bash "$L4/replay.sh" "$R5/replay" "$CORPUS"
leg I3ab bash "$L4/iparm.sh" "$R5/iparm"
leg I3c bash "$R5/legs/interior.sh" "$R5/interior" "$CORPUS"
leg F3 bash "$L4/install_smoke.sh" "$R5/install-smoke"
leg I4 bash "$D3/r4/sl/flip_enum.sh" "$R5/identity" "$LANE"
leg I2 bash "$L4/census.sh" "$R5/census" "$CORPUS"
echo "ALL_LEGS_PASSED"
echo "FOREGROUND_END $(date -Is)"
