#!/usr/bin/env bash
# RF-1 gate A+B2 mutation leg (../DECLARATION-RF-1-AB2.md section 5): gate C mutations.sh with the table (mutate_ab2.py) and
# test filters replaced. Control, then each mutation of mutate_ab2.py, rebuilt and run on the pinning tests; the tree is
# restored after each. KILLED iff a pinning test fails. R-ids are reported, not asserted.
# usage: mutations.sh <out> <tree> <build dir> <ids...>   Run under: ~/.local/bin/box-lane -- bash mutations.sh ...
set -o pipefail
OUT=${1:?out dir}; TREE=${2:?tree}; B=${3:?build dir}; shift 3; IDS="$*"
L=/home/ghecht/Projects/hven/.claude/worktrees/rf1/docs/notes/data/2026-11-m8-rf1/legs
export MUT_TREE=$TREE MKL_NUM_THREADS=1 CCACHE_DISABLE=1 TMPDIR=$TREE/.scratch/tmp GIT_CEILING_DIRECTORIES=$TREE/.scratch/tmp
mkdir -p "$TMPDIR"; OUT=$(realpath -m "$OUT"); mkdir -p "$OUT"
echo "FOREGROUND_START $(date -Is)"
cd "$TREE" || exit 1
git diff --quiet --ignore-submodules=all -- src include tests || { echo TREE_DIRTY_BEFORE; exit 2; }
for m in $IDS; do python3 "$L/mutate_ab2.py" "$m" > /dev/null || { echo "PREFLIGHT_DOES_NOT_APPLY $m"; python3 "$L/mutate_ab2.py" restore; exit 2; }; python3 "$L/mutate_ab2.py" restore > /dev/null; done
echo "PREFLIGHT every mutation applies and restores: [$IDS]"
TARGETS="hven_sqp_tests hven_sqp_seam_tests hven_trace_tests"
run_one() { local id=$1 r=0 r1 r2 r3
  [ "$id" = control ] || python3 "$L/mutate_ab2.py" "$id" > /dev/null || return 9
  [ "$id" = control ] || git diff -- src include > "$OUT/$id.diff"
  timeout 2400 ninja -C "$B" -j${BOX_LANE_JOBS:-4} $TARGETS > "$OUT/$id.build.log" 2>&1 || { echo "$id BUILD_FAIL"; python3 "$L/mutate_ab2.py" restore; return 8; }
  { timeout 1200 "$B/tests/sqp/hven_sqp_tests" --gtest_filter='Rf1*:RestorationBestOrder.*:QpCap*:QpFailureStep.*:SqpSolverQpFailure.*:SqpSolverRestoration*:RestorationResumeReduction*:Continuation.ProbeBudget*:HsSweepCorpus.*'; r1=$?
    timeout 1200 "$B/tests/sqp/hven_sqp_seam_tests" --gtest_filter='SqpLadderSeam.*:Rf1*'; r2=$?
    timeout 600 "$B/tests/trace/hven_trace_tests" --gtest_filter='TraceMessagesSqp.*'; r3=$?; } > "$OUT/$id.tests.log" 2>&1
  r=$((r1 + r2 + r3))
  grep -a "^\[  FAILED  \] [A-Za-z]" "$OUT/$id.tests.log" | sed 's/^.*\] //; s/ (.*//' | sort -u > "$OUT/$id.killers.txt"
  [ "$id" = control ] || python3 "$L/mutate_ab2.py" restore > /dev/null
  echo "MUT $id rc=$r killers=$(wc -l < "$OUT/$id.killers.txt")"; sed 's/^/   /' "$OUT/$id.killers.txt"
  case "$id" in control) [ $r -eq 0 ] ;; R*) true ;; *) [ $r -ne 0 ] ;; esac
}
verdict=MUTATIONS_AS_DECLARED
run_one control || verdict=CONTROL_NOT_CLEAN
for m in $IDS; do run_one $m || verdict="MUTATION_SURVIVED_$m"; done
timeout 2400 ninja -C "$B" -j${BOX_LANE_JOBS:-4} $TARGETS > "$OUT/final.build.log" 2>&1
git diff --quiet --ignore-submodules=all -- src include tests || { echo TREE_NOT_RESTORED; verdict=TREE_NOT_RESTORED; }
echo "VERDICT $verdict"; echo "FOREGROUND_END $(date -Is)"
[ "$verdict" = MUTATIONS_AS_DECLARED ]
