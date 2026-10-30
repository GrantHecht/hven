#!/usr/bin/env bash
# THE RST GATES-RUN PRE-FLIGHT: COPIED from preflight-scl.sh and re-pointed (baseline = the SCL record; falsifiers re-derived for this range). Its header follows.
# THE SCL GATES-RUN PRE-FLIGHT, as a script: its stdout is preflight-scl.txt (this directory). Run at the declaration state (the wrappers
# files uncommitted on top of c1075641, so every derivation sees c1075641's sources), every slot DERIVED or MEASURED on d9bfbb27..c1075641.
# Nothing here touches a build tree or runs a leg. Paths and the range are DERIVED from this file's location and the runner.
#  A --preflight-only; B --check-g4 on the rehearsal pin arms (+ the exact-arm comparator has teeth); C --check-names on the rehearsal ctest -N
#  listing (+ falsifiers); D --check-skips (+ falsifier); E wrong-BEFORE / sentinel / class A / Debug count / removed / fixture / universe-added /
#  class B / library-class-B falsifiers; F classifier.
W=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd) || exit 3
ROOT=$(git -C "$W" rev-parse --show-toplevel) || exit 3
R=$W/gates-rst.sh
REH=$W/rehearsal
cd "$ROOT" || exit 3
run() { local label="$1"; shift; echo; echo "=== $label"; "$@" 2>&1; echo "rc=$?"; }
BEFORE_S=$(sed -n 's/^BEFORE=\${BEFORE:-\([0-9a-f]*\)}$/\1/p' "$R")
PREVREC=$(git log -1 --format=%h --diff-filter=A "$BEFORE_S" -- 'docs/notes/data/*/gates/suite-release.txt')
echo "# range $BEFORE_S..$(git rev-parse --short HEAD) (HEAD sources == 3e90d587's); by-name baseline record commit $PREVREC"
run "A: --preflight-only" bash "$R" --preflight-only
run "B: --check-g4 (pin arms of the rehearsal comparison)" bash "$R" --check-g4 "$REH/rehearsal-rst-g4-pin-arms.txt"
echo; echo "=== B2: the exact arm has teeth (the comparator exits non-zero on any difference): the M6 pin vs the previous record, EXACT, no gate"
PIN=$ROOT/docs/notes/data/2026-09-m6-close-gate/replay
PREVD=$ROOT/docs/notes/data/2026-10-m7-scl/gates/replay
CMPOUT=$(mktemp)
python3 -B scripts/compare_replay.py "$PIN/head-walk.csv" "$PREVD/head-walk.csv" "falsifier: M6 pin vs SCL record, EXACT" > "$CMPOUT" 2>&1; rc=$?
head -1 "$CMPOUT"; echo "rc=$rc (non-zero: a real difference fails the asserted exact arm)"
python3 -B scripts/compare_replay.py "$PREVD/head-walk.csv" "$PREVD/head-walk.csv" "control: SCL record vs itself, EXACT" > "$CMPOUT" 2>&1; rc=$?
head -1 "$CMPOUT"; echo "rc=$rc (zero)"
rm -f "$CMPOUT"
run "C: --check-names release on the rehearsal listing" bash "$R" --check-names release "$REH/ctest-N-startfmt-derived.txt"
LONGLESS=$(mktemp); grep -v "Continuation.ProbeBudgetBoundsAFailingProposal" "$REH/ctest-N-startfmt-derived.txt" > "$LONGLESS"
run "C2: --check-names debug on the listing less the long test (G1 excludes it by -E)" bash "$R" --check-names debug "$LONGLESS"
SHORT=$(mktemp); head -n -1 "$REH/ctest-N-startfmt-derived.txt" > "$SHORT"
run "C3 falsifier: listing short one test" bash "$R" --check-names release "$SHORT"
BASEREL=$ROOT/docs/notes/data/2026-10-m7-scl/gates/suite-release.txt
run "C4 falsifier: the previous record's own suite (the baseline) is refused" bash "$R" --check-names release "$BASEREL"
run "D: --check-skips baseline vs baseline (debug)" bash "$R" --check-skips debug "$ROOT/docs/notes/data/2026-10-m7-scl/gates/suite-debug.txt"
run "D1: --check-skips baseline vs baseline (release)" bash "$R" --check-skips release "$BASEREL"
run "D2 falsifier: release log vs debug expectation" bash "$R" --check-skips debug "$BASEREL"
run "E1 falsifier: wrong BEFORE (d9bfbb27, the SCL base)" env BEFORE=d9bfbb27 bash "$R" --preflight-only
run "E2 falsifier: UNMEASURED sentinel in a class A slot" env EXPECT_PSYM_CLASS_A=UNMEASURED bash "$R" --preflight-only
run "E3 falsifier: class A declared 6/6" env EXPECT_PSYM_CLASS_A=6/6 bash "$R" --preflight-only
run "E4 falsifier: Debug test count 3190 (one ADDED name short)" env EXPECT_DEBUG_TESTS=3190 bash "$R" --preflight-only
echo; echo "=== E5 falsifier: a REMOVED name declared that the range does not remove (a scratch copy of the runner: REMOVED_TEST_NAMES is not an environment slot)"
E5=$ROOT/.scratch/runner-e5-copy.sh; mkdir -p "$ROOT/.scratch"
sed "s|^REMOVED_TEST_NAMES=''|REMOVED_TEST_NAMES='FunnelHType.RetractionUndoesOnlyTheLastJudgement'|" "$R" > "$E5"
grep -c "^REMOVED_TEST_NAMES='FunnelHType" "$E5"
bash "$E5" --preflight-only 2>&1 | tail -2; echo "rc=${PIPESTATUS[0]}"; rm -f "$E5"
run "E6 falsifier: an ADDED fixture declared that the range does not add" env ADDED_TEST_FILES=tests/qp/fixtures/e2-d4/RLA_BRITGAS_qp0.txt bash "$R" --preflight-only
run "E7 falsifier: an added in-universe source declared that the range does not add (EXPECT_PSYM_ADDED=1)" env EXPECT_PSYM_ADDED=1 bash "$R" --preflight-only
run "E7b falsifier: a source declared ADDED outside the universe that the range does not add" env EXPECT_ADDED_OUTSIDE_UNIVERSE=tests/sqp/test_sqp_ladder_seams.cpp bash "$R" --preflight-only
run "E10 falsifier: the outside-universe modified source (test_sqp_ladder_seams.cpp) not declared" env EXPECT_CLASS_A_OUTSIDE_SRCS= bash "$R" --preflight-only
run "E7c falsifier: tests/sqp/CMakeLists.txt add declared that the range does not make" env EXPECT_SQP_CMAKE_ADDS=test_column_scaling.cpp bash "$R" --preflight-only
echo; echo "=== E8 falsifier: a class B name declared whose source is in the range (tests/sqp test_trace_writer.cpp.o swapped in for one declared object)"
RELB=$(sed -n 's/^EXPECT_CLASS_B_NAMES_RELEASE=\${EXPECT_CLASS_B_NAMES_RELEASE-"\(.*\)"}$/\1/p' "$R")
SWAPPED=${RELB/tests\/drivers\/CMakeFiles\/hven_drivers_tests.dir\/test_warm_protocol.cpp.o/tests\/sqp\/CMakeFiles\/hven_sqp_tests.dir\/test_trace_writer.cpp.o}
[ "$SWAPPED" != "$RELB" ] || echo "FALSIFIER_NOT_APPLIED: the swap target is not in the declared list"
env EXPECT_CLASS_B_NAMES_RELEASE="$SWAPPED" bash "$R" --preflight-only 2>&1 | tail -3; echo "rc=${PIPESTATUS[0]}"
echo; echo "=== E9 falsifier: sqp_solver.cpp.o (class A) declared as library class B"
LIBB=$(sed -n 's/^EXPECT_LIB_CLASS_B_RELEASE=\${EXPECT_LIB_CLASS_B_RELEASE-"\(.*\)"}$/\1/p' "$R")
env EXPECT_LIB_CLASS_B_RELEASE="$LIBB CMakeFiles/hven.dir/src/drivers/sqp_solver.cpp.o" bash "$R" --preflight-only 2>&1 | tail -3; echo "rc=${PIPESTATUS[0]}"
echo; echo "=== F: classifier byte-identical to the previous record's"
cmp "$W/classify_psym_m4_uncapped.py" "$ROOT/docs/notes/data/2026-10-m7-scl/wrappers-gates/classify_psym_m4_uncapped.py" && echo "cmp SCL: IDENTICAL"
rm -f "$LONGLESS" "$SHORT"
