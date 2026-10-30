#!/usr/bin/env bash
# THE WX2 GATES-RUN PRE-FLIGHT: COPIED from preflight-wx.sh (NOT edited in place) and re-pointed (by-name baseline = the WX gates record fcd2574f; G4 exact arm and E1 OFF baseline = the WX gates record's replay/ and e1/, DERIVED from BEFORE by the runner; the ADDED name list is EMPTY (no test is added or removed: checked against git); falsifiers re-derived for this range; new falsifiers W1-W9 for the structural additions W2-S1..S7: the NO-OBJECT probe source, the empty ADDED list, the tests/CMakeLists.txt WX-R13 block, the UNITY class A sources, the reference-dispatch and probe-object assertions, and the object census). The WX header follows.
# THE WX GATES-RUN PRE-FLIGHT: COPIED from preflight-a3.sh (NOT edited in place) and re-pointed (by-name baseline = the A3 gates record; G4 exact arm and E1 OFF baseline = the A3 gates record's replay/ and e1/, DERIVED from BEFORE by the runner; the E1 comparator's declared added-column set is EMPTY; falsifiers re-derived for this range: E4 count, E5b names, E8 (class B declared empty -> a modified-source object swapped in), E10, E11, B3 variants; leg G is now the IDENTICAL/NOISE-ONLY class A proof, check-class-a-wx2.sh, because this range has no Debug-only class B). The A3 header follows.
# THE A3 GATES-RUN PRE-FLIGHT: COPIED from preflight-a2.sh and re-pointed (by-name baseline = the A2 gates record; G4 exact arm and E1 OFF baseline = the toolchain re-baseline record, DERIVED from BEFORE by the runner; falsifiers re-derived for this range: no TEST_F and no outside-universe source in the range, so the TEST_F-blind falsifier E5c is dropped and E10 is turned round (an outside source declared that the range does not modify); new B3 proves the E1 OFF comparator has teeth). The A2 header follows.
# THE A2 GATES-RUN PRE-FLIGHT: COPIED from preflight-scw.sh and re-pointed (baseline = the SCW gates record; falsifiers re-derived for this range: E5b now omits a declared ADDED name; E5c a TEST_F-blind line reader; G is now the Debug-only class B admission proof plus the mirror-block identity check). The SCW header follows.
# THE SCW GATES-RUN PRE-FLIGHT: COPIED from preflight-rst.sh and re-pointed (baseline = the RST gates record; falsifiers re-derived for this range; plus the mirror-admission proof and four new falsifiers E5b/E11/E12/G). The RST header (itself a copy of the SCL one) follows.
# THE SCL GATES-RUN PRE-FLIGHT, as a script: its stdout is preflight-scl.txt (this directory). Run at the declaration state (the wrappers
# files uncommitted on top of c1075641, so every derivation sees c1075641's sources), every slot DERIVED or MEASURED on d9bfbb27..c1075641.
# Nothing here touches a build tree or runs a leg. Paths and the range are DERIVED from this file's location and the runner.
#  A --preflight-only; B --check-g4 on the rehearsal pin arms (+ the exact-arm comparator has teeth); C --check-names on the rehearsal ctest -N
#  listing (+ falsifiers); D --check-skips (+ falsifier); E wrong-BEFORE / sentinel / class A / Debug count / removed / fixture / universe-added /
#  class B / library-class-B falsifiers; F classifier.
W=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd) || exit 3
ROOT=$(git -C "$W" rev-parse --show-toplevel) || exit 3
R=$W/gates-wx2.sh
REH=$W/rehearsal
cd "$ROOT" || exit 3
run() { local label="$1"; shift; echo; echo "=== $label"; "$@" 2>&1; echo "rc=$?"; }
BEFORE_S=$(sed -n 's/^BEFORE=\${BEFORE:-\([0-9a-f]*\)}$/\1/p' "$R")
PREVREC=$(git log -1 --format=%h --diff-filter=A "$BEFORE_S" -- 'docs/notes/data/*/gates/suite-release.txt')
echo "# range $BEFORE_S..$(git rev-parse --short HEAD) (HEAD sources == aaa95642's); by-name baseline record commit $PREVREC"
run "A: --preflight-only" bash "$R" --preflight-only
run "B: --check-g4 (pin arms of the rehearsal comparison)" bash "$R" --check-g4 "$REH/rehearsal-wx2-g4-pin-arms.txt"
TCB=$ROOT/docs/notes/data/2026-10-m7-toolchain-2026-10-27
PREVR=$ROOT/docs/notes/data/2026-10-m7-wx-gates
run "B1: --check-g4 on the toolchain record's own comparison-vs-m6-pin.txt (the declared 43-set equals the new-toolchain baseline's measured set)" bash "$R" --check-g4 "$TCB/replay/comparison-vs-m6-pin.txt"
echo; echo "=== B2: the exact arm has teeth (the comparator exits non-zero on any difference): the M6 pin vs the previous record, EXACT, no gate"
PIN=$ROOT/docs/notes/data/2026-09-m6-close-gate/replay
PREVD=$PREVR/gates/replay
CMPOUT=$(mktemp)
python3 -B scripts/compare_replay.py "$PIN/head-walk.csv" "$PREVD/head-walk.csv" "falsifier: M6 pin vs the WX record, EXACT" > "$CMPOUT" 2>&1; rc=$?
head -1 "$CMPOUT"; echo "rc=$rc (non-zero: a real difference fails the asserted exact arm)"
python3 -B scripts/compare_replay.py "$PREVD/head-walk.csv" "$PREVD/head-walk.csv" "control: the WX record vs itself, EXACT" > "$CMPOUT" 2>&1; rc=$?
head -1 "$CMPOUT"; echo "rc=$rc (zero)"
rm -f "$CMPOUT"
run "C: --check-names release on the rehearsal listing" bash "$R" --check-names release "$REH/ctest-N-startfmt-derived.txt"
LONGLESS=$(mktemp); grep -v "Continuation.ProbeBudgetBoundsAFailingProposal" "$REH/ctest-N-startfmt-derived.txt" > "$LONGLESS"
run "C2: --check-names debug on the listing less the long test (G1 excludes it by -E)" bash "$R" --check-names debug "$LONGLESS"
SHORT=$(mktemp); head -n -1 "$REH/ctest-N-startfmt-derived.txt" > "$SHORT"
run "C3 falsifier: listing short one test" bash "$R" --check-names release "$SHORT"
BASEREL=$ROOT/docs/notes/data/2026-10-m7-wx-gates/gates/suite-release.txt
run "C4 control: the previous record's own suite IS this range's name set (the range adds and removes no test), so it is accepted" bash "$R" --check-names release "$BASEREL"
RENAMED=$(mktemp); sed '0,/NonNullNonEmptyMatchesProjectVersion/s//NonNullNonEmptyMatchesProjectVersionX/' "$BASEREL" > "$RENAMED"
run "C4b falsifier: the baseline with ONE test renamed is refused (same count, different name)" bash "$R" --check-names release "$RENAMED"
rm -f "$RENAMED"
run "D: --check-skips baseline vs baseline (debug)" bash "$R" --check-skips debug "$ROOT/docs/notes/data/2026-10-m7-wx-gates/gates/suite-debug.txt"
run "D1: --check-skips baseline vs baseline (release)" bash "$R" --check-skips release "$BASEREL"
run "D2 falsifier: release log vs debug expectation" bash "$R" --check-skips debug "$BASEREL"
run "E1 falsifier: wrong BEFORE (1ff37db2, the WX base)" env BEFORE=1ff37db2 bash "$R" --preflight-only
run "E2 falsifier: UNMEASURED sentinel in a class A slot" env EXPECT_PSYM_CLASS_A=UNMEASURED bash "$R" --preflight-only
run "E3 falsifier: class A declared 6/6" env EXPECT_PSYM_CLASS_A=6/6 bash "$R" --preflight-only
run "E4 falsifier: Debug test count 3230 (one short of the declared 3231 = 3231 + 0 ADDED - 0 REMOVED)" env EXPECT_DEBUG_TESTS=3230 bash "$R" --preflight-only
echo; echo "=== E5 falsifier: a REMOVED name declared that the range does not remove (a scratch copy of the runner: REMOVED_TEST_NAMES is not an environment slot)"
E5=$ROOT/.scratch/runner-e5-copy.sh; mkdir -p "$ROOT/.scratch"
sed "s|^REMOVED_TEST_NAMES=''|REMOVED_TEST_NAMES='FunnelHType.RetractionUndoesOnlyTheLastJudgement'|" "$R" > "$E5"
grep -c "^REMOVED_TEST_NAMES='FunnelHType" "$E5"
bash "$E5" --preflight-only 2>&1 | tail -2; echo "rc=${PIPESTATUS[0]}"; rm -f "$E5"
echo; echo "=== E5b falsifier (W2-S2): a name declared ADDED that the range does not add (a scratch copy; ADDED_TEST_NAMES is not an environment slot): the empty list is checked against git, so a non-empty one is refused"
sed "s|^ADDED_TEST_NAMES=''|ADDED_TEST_NAMES='HsSweepRepair.WarmChainOnAQuarterGridCertifiesNoPointOffTheRow'|" "$R" > "$E5"
grep -c "^ADDED_TEST_NAMES='HsSweepRepair" "$E5"
bash "$E5" --preflight-only 2>&1 | tail -2; echo "rc=${PIPESTATUS[0]}"; rm -f "$E5"
echo; echo "=== E5d (W2-S2) control: the range adds no TEST line, by git"
git diff "$(sed -n 's/^BEFORE=\${BEFORE:-\([0-9a-f]*\)}$/\1/p' "$R")..HEAD" -- 'tests/*.cpp' | grep -cE '^[+-]TEST(_[FP])?\('
echo; echo "=== E5c (A2) DROPPED: the TEST_F-blind reader falsifier has nothing to refuse in this range (no TEST_F is added; the widened TEST(_[FP])? regex is carried unchanged from A2, where E5c proved it necessary)"
run "E6 falsifier: an ADDED fixture declared that the range does not add" env ADDED_TEST_FILES=tests/qp/fixtures/e2-d4/RLA_BRITGAS_qp0.txt bash "$R" --preflight-only
run "E7 falsifier: an added in-universe source count declared that the range does not add (EXPECT_PSYM_ADDED=2)" env EXPECT_PSYM_ADDED=2 bash "$R" --preflight-only
run "E7b falsifier: a source declared ADDED outside the universe that the range does not add" env EXPECT_ADDED_OUTSIDE_UNIVERSE=tests/sqp/test_ipqp_seams.cpp bash "$R" --preflight-only
run "E10 falsifier: an outside-universe MODIFIED source declared that the range does not modify (tests/qp/test_qp_degeneracy_arm.cpp)" env EXPECT_CLASS_A_OUTSIDE_SRCS="tests/qp/test_qp_degeneracy_arm.cpp" bash "$R" --preflight-only
run "E7c falsifier: tests/sqp/CMakeLists.txt add declared that the range does not make" env EXPECT_SQP_CMAKE_ADDS=test_column_scaling.cpp bash "$R" --preflight-only
echo; echo "=== E8 falsifier: a class B name declared whose source is in the range (this range declares class B EMPTY, so the falsifier declares ONE object, test_sqp_solver.cpp.o, whose source is MODIFIED, with class B 1/1 and DIFFERING 3/3 so the count checks pass first)"
env EXPECT_PSYM_DIFFERING=3/3 EXPECT_PSYM_CLASS_B=1/1 EXPECT_CLASS_B_NAMES_RELEASE="tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_sqp_solver.cpp.o" EXPECT_CLASS_B_NAMES_DEBUG="tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_sqp_solver.cpp.o" bash "$R" --preflight-only 2>&1 | tail -3; echo "rc=${PIPESTATUS[0]}"
echo; echo "=== E9 falsifier: non_linear_program.cpp.o (class A, MODIFIED source) declared as library class B (the declared list is empty)"
env EXPECT_LIB_CLASS_B_RELEASE="CMakeFiles/hven.dir/src/model/non_linear_program.cpp.o" bash "$R" --preflight-only 2>&1 | tail -3; echo "rc=${PIPESTATUS[0]}"
echo; echo "=== B3: the E1 OFF comparator has teeth (synthetic heads built from the A3 record's e1-off.csv; the declared added-column set is EMPTY)"
E1B=$PREVR/gates/e1/e1-off.csv; E1CMP=$W/e1_off_compare_wx2.py; T3=$(mktemp -d)
python3 -B - "$E1B" "$T3" <<'PYEOF'
import sys
src, d = sys.argv[1], sys.argv[2]
lines = open(src).read().split("\n")
hdr_i = next(i for i, l in enumerate(lines) if not l.startswith("#") and l.strip())
def variant(name, add=None, mutate=None, drop=None):
    out = []
    for i, l in enumerate(lines):
        if i < hdr_i or not l.strip():
            out.append(l); continue
        c = l.split(",")
        if i == hdr_i:
            if add: c.append(add)
            if drop is not None: dropped = c.pop(drop)
        else:
            if add: c.append("0")
            if mutate and i == hdr_i + 1: c[mutate] = c[mutate] + "x"
            if drop is not None: c.pop(drop)
        out.append(",".join(c))
    open(f"{d}/{name}.csv", "w").write("\n".join(out))
variant("good")
variant("mutated", mutate=5)
variant("added", add="some_new_column")
variant("dropped", drop=7)
PYEOF
for v in good mutated added dropped; do
  python3 -B "$E1CMP" "$E1B" "$T3/$v.csv" "" | tail -1; echo "$v rc=${PIPESTATUS[0]} (want: good 0; mutated 1; added 1; dropped 1)"
done
python3 -B "$E1CMP" "$E1B" "$T3/added.csv" some_new_column | tail -1; echo "added-and-declared rc=${PIPESTATUS[0]} (want 0: the comparator still accepts a declared column that is 0 on every row)"
rm -rf "$T3"
echo; echo "=== F: classifier byte-identical to the WX record's"
cmp "$W/classify_psym_m4_uncapped.py" "$ROOT/docs/notes/data/2026-10-m7-wx-gates/wrappers-gates/classify_psym_m4_uncapped.py" && echo "cmp WX: IDENTICAL"
run "E11 falsifier: class B declared 1/0 (DIFFERING 2/2 != A + B)" env EXPECT_PSYM_CLASS_B=1/0 bash "$R" --preflight-only
run "E12 falsifier: a MODIFIED library source (get_core_count.cpp.o) declared a Debug NOISE-ONLY library mover" env EXPECT_LIB_NOISE_MOVERS_DEBUG="CMakeFiles/hven.dir/src/interior/utils/get_core_count.cpp.o" bash "$R" --preflight-only
echo; echo "=== W: the WX2 structural additions W2-S1..S6, each with a falsifier the runner refuses"
run "W1 falsifier (W2-S1): a NO-OBJECT source declared that the range does not add (tests/qp/test_qp_rank_repair.cpp is MODIFIED, not ADDED)" env EXPECT_ADDED_NO_OBJECT=tests/qp/test_qp_rank_repair.cpp EXPECT_ADDED_CXX=1 bash "$R" --preflight-only
run "W1b falsifier (W2-S1): the probe is ADDED but its command is declared add_executable (it sits in try_run)" env NO_OBJECT_COMMAND=add_executable bash "$R" --preflight-only
run "W1c falsifier (W2-S1): the NO-OBJECT slot emptied, so the probe is an ADDED .cpp with no object (ADDED declared 0 != derived 1)" env EXPECT_ADDED_NO_OBJECT= bash "$R" --preflight-only
run "W3 falsifier (W2-S3): the WX-R13 block declared one line short (44 != 45)" env EXPECT_CM_BLOCK_LINES=44 bash "$R" --preflight-only
run "W6 falsifier: an ADDED test header declared that the range does not add (route_reference.h is MODIFIED, not added)" env ADDED_TEST_FILES=tests/common_support/route_reference.h bash "$R" --preflight-only
run "W4 falsifier (W2-S6): a UNITY source declared that is NOT modified (test_watchdog.cpp swapped for tests/interior/test_assembly_allocation.cpp; the count of six is kept so the arithmetic passes first)" env EXPECT_CLASS_A_UNITY_SRCS="tests/interior/test_filter_acceptance.cpp tests/interior/test_funnel_acceptance.cpp tests/interior/test_l1_restoration.cpp tests/interior/test_merit_rules.cpp tests/interior/test_monitored_governor.cpp tests/interior/test_assembly_allocation.cpp" bash "$R" --preflight-only
run "W4b falsifier (W2-S6): a UNITY source that is not in a unity target (test_watchdog.cpp swapped for tests/sqp/test_sqp_solver.cpp: modified, but hven_sqp_tests is not UNITY_BUILD)" env EXPECT_CLASS_A_UNITY_SRCS="tests/interior/test_filter_acceptance.cpp tests/interior/test_funnel_acceptance.cpp tests/interior/test_l1_restoration.cpp tests/interior/test_merit_rules.cpp tests/interior/test_monitored_governor.cpp tests/sqp/test_sqp_solver.cpp" bash "$R" --preflight-only
run "W5 falsifier (W2-S6): the UNITY slot emptied, so six modified sources are unaccounted for (class A arithmetic)" env EXPECT_CLASS_A_UNITY_SRCS= bash "$R" --preflight-only
run "W8 falsifier (W2-S7): the census C counts are still the UNMEASURED sentinel" env EXPECT_CENSUS_C_PURE_DIFFERING=UNMEASURED bash "$R" --preflight-only
echo; echo "=== W7: the reference-dispatch and probe-object assertion helpers (W2-S4/S5), cut out of the runner and run on fabricated logs"
FN=$(awk '/^assert_reference_dispatch\(\) \{/ {f=1} f {print} /^assert_no_probe_object\(\) \{/ {g=1} g && /^\}/ {exit}' "$R")
T7=$(mktemp -d)
printf -- '-- hven tests: MKL dispatch Intel AVX2 FMA -> REFERENCE (route pins asserted)\n' > "$T7/ok.log"
printf -- '-- hven tests: MKL dispatch AVX512 -> OFF-REFERENCE (portable arms)\n' > "$T7/off.log"
printf -- '-- hven tests: MKL dispatch X -> REFERENCE (route pins asserted)\n-- hven tests: MKL dispatch X -> REFERENCE (route pins asserted)\n' > "$T7/twice.log"
printf -- '-- nothing here\n' > "$T7/none.log"
printf -- '[1/2] Building CXX object a.o\n' > "$T7/build-ok.log"; printf -- '[1/2] Building CXX object tests/common_support/mkl_dispatch_probe.cpp.o\n' > "$T7/build-bad.log"
( eval "$FN"
  for l in ok off twice none; do assert_reference_dispatch "W7-$l" "$T7/$l.log" > /dev/null; echo "reference-dispatch $l rc=$? (want: ok 0; off 1; twice 1; none 1)"; done
  assert_no_probe_object "W7-b-ok" "$T7/build-ok.log" > /dev/null; echo "no-probe build-ok rc=$? (want 0)"
  assert_no_probe_object "W7-b-bad" "$T7/build-bad.log" > /dev/null; echo "no-probe build-bad rc=$? (want 1)" )
rm -rf "$T7"
echo; echo "=== G: the class A NOISE-ONLY / IDENTICAL admission, proven offline on the rehearsal's own classifications and compares (this range has no Debug-only class B: EXPECT_PSYM_B_DEBUG_ONLY and the mirror slot are EMPTY)"
bash "$REH/check-class-a-wx2.sh" 2>&1; echo "rc=$?"
rm -f "$LONGLESS" "$SHORT"
