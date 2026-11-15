#!/usr/bin/env bash
# The legs, in order: F1-rel, F1-dbg, I1, I4(A), I4(B) + tally, M. Stops at the first failed leg.
# usage: orchestrate.sh <code head> <armA corpus sha256> <armB corpus sha256> <lane A sha256> <lane B sha256>
# Run as: flock /tmp/box-build.lock bash orchestrate.sh ... > ../legs-out/orchestrate.out 2>&1
set -o pipefail
. "$(dirname "$0")/env.sh"
L=$E/legs; O=$E/legs-out
HEADSHA=${1:?code head}; SA=${2:?}; SB=${3:?}; LA=${4:?}; LB=${5:?}
pgrep_guard
echo "FOREGROUND_START $(date -Is)"
git -C "$WT" diff --quiet "$HEADSHA" -- src include tests bench examples CMakeLists.txt cmake scripts || { echo SOURCE_MOVED_SINCE_CODE_HEAD; exit 2; }
[ -z "$(git -C "$WT" status --porcelain --untracked-files=no -- src include tests bench examples CMakeLists.txt cmake scripts)" ] || { echo SOURCE_DIRTY; exit 2; }
mkdir -p "$TMPDIR" "$O/suites"
leg() { local name=$1; shift; echo "LEG $name start $(date -Is)"; "$@"; local rc=$?; echo "LEG $name rc=$rc $(date -Is)";
        [ $rc -eq 0 ] || { echo "STOPPED_AT $name"; echo "FOREGROUND_END $(date -Is)"; exit $rc; }; }
leg F1-rel bash "$L/ctest.sh" rel "$O/suites/ctest-rel.log"
leg F1-dbg bash "$L/ctest.sh" dbg "$O/suites/ctest-dbg.log"
leg I1 bash "$L/replay.sh" "$O/replay" "$SA" "$SB"
leg I4-A bash "$E/sl/enum.sh" A "$B/identity-A" "$LA"
leg I4-B bash "$E/sl/enum.sh" B "$B/identity-B" "$LB"
leg I4-tally python3 "$E/sl/tally.py" "$B/identity-A" "$B/identity-B" "$ROSTER" "$E/declared-movers.txt" "$B/dev-A" "$B/dev-B" > "$O/tally.txt" 2>&1
tail -40 "$O/tally.txt"
leg M bash "$L/mutate.sh" "$HEADSHA" "$O/mutation"
echo ALL_LEGS_PASSED
echo "FOREGROUND_END $(date -Is)"
