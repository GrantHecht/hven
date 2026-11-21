#!/usr/bin/env bash
# PF-2e mutation leg (../DECLARATION-PF-2e.md section 4; pf2d's mutations.sh re-pointed, plus the seam executable): control,
# then every listed mutation of mutate.py, each rebuilt (hven_qp_tests, hven_qp_seam_tests) and run on the pinning tests;
# the tree is restored after each. KILLED iff a pinning test fails. The tree must be clean (committed) before it starts.
# usage: mutations.sh <out> <tree> <build dir> <ids...>   Run under: ~/.local/bin/box-lane -- bash mutations.sh ...
set -o pipefail
OUT=${1:?out dir}; TREE=${2:?tree}; B=${3:?build dir}; shift 3; IDS="$*"
L=/home/ghecht/Projects/hven/.claude/worktrees/pf2e/docs/notes/data/2026-11-m8-pf2e/legs
export MUT_TREE=$TREE MKL_NUM_THREADS=1 CCACHE_DISABLE=1 TMPDIR=$TREE/.scratch/tmp GIT_CEILING_DIRECTORIES=$TREE/.scratch/tmp
mkdir -p "$TMPDIR"; OUT=$(realpath -m "$OUT"); mkdir -p "$OUT"
echo "FOREGROUND_START $(date -Is)"
cd "$TREE" || exit 1
git diff --quiet --ignore-submodules=all -- src include || { echo TREE_DIRTY_BEFORE; exit 2; }
for m in $IDS; do python3 "$L/mutate.py" "$m" > /dev/null || { echo "PREFLIGHT_DOES_NOT_APPLY $m"; python3 "$L/mutate.py" restore; exit 2; }; python3 "$L/mutate.py" restore > /dev/null; done
echo "PREFLIGHT every mutation applies and restores: [$IDS]"
SEAM='QpE2WorkingSet.AGreyReReadSolvesTheAcopr57RestorationQp:QpE2WorkingSet.TheUndecidableBorderPingPongRetreatsOnceAndEndsOptimal:QpLatchReason.BritgasMainSubproblemHoldsItsConditioningLatch:QpE2WorkingSet.AConvergedGreyPinOnDallasmIsReReadDependent'
run_one() { local id=$1 r=0 r2=0
  [ "$id" = control ] || python3 "$L/mutate.py" "$id" || return 9
  [ "$id" = control ] || git diff -- src include > "$OUT/$id.diff"
  timeout 2400 ninja -C "$B" -j${BOX_LANE_JOBS:-4} hven_qp_tests hven_qp_seam_tests > "$OUT/$id.build.log" 2>&1 || { echo "$id BUILD_FAIL"; python3 "$L/mutate.py" restore; return 8; }
  timeout 900 "$B/tests/qp/hven_qp_tests" --gtest_filter='WsIndependence*:QpGreyReread*' > "$OUT/$id.tests.log" 2>&1; r=$?
  timeout 900 "$B/tests/qp/hven_qp_seam_tests" --gtest_filter="$SEAM" > "$OUT/$id.seam.log" 2>&1; r2=$?
  cat "$OUT/$id.tests.log" "$OUT/$id.seam.log" | grep -a "^\[  FAILED  \] [A-Za-z]" | sed 's/^.*\] //; s/ (.*//' | sort -u > "$OUT/$id.killers.txt"
  [ "$id" = control ] || python3 "$L/mutate.py" restore > /dev/null
  echo "MUT $id rc=$r seam_rc=$r2 killers=$(wc -l < "$OUT/$id.killers.txt")"; sed 's/^/   /' "$OUT/$id.killers.txt"
  if [ "$id" = control ]; then [ $r -eq 0 ] && [ $r2 -eq 0 ]; else [ $r -ne 0 ] || [ $r2 -ne 0 ]; fi
}
verdict=MUTATIONS_AS_DECLARED
run_one control || verdict=CONTROL_NOT_CLEAN
for m in $IDS; do run_one $m || verdict="MUTATION_SURVIVED_$m"; done
timeout 2400 ninja -C "$B" -j${BOX_LANE_JOBS:-4} hven_qp_tests hven_qp_seam_tests > "$OUT/final.build.log" 2>&1
git diff --quiet --ignore-submodules=all -- src include || { echo TREE_NOT_RESTORED; verdict=TREE_NOT_RESTORED; }
echo "VERDICT $verdict"; echo "FOREGROUND_END $(date -Is)"
[ "$verdict" = MUTATIONS_AS_DECLARED ]
