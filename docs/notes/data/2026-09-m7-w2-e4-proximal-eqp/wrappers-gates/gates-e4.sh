#!/usr/bin/env bash
# M7 W2 E4 (THE PROXIMAL EQP) + FIX ROUNDS 1 AND 2 -- THE GATES on ONE code commit, one
# declaration. E4 replaces the ordinary EQP step with a PROXIMAL pair and lands three
# mechanisms fix round 2 added on top of it: the step's ROW-DRIFT CHECK (a step may not
# leave a working row the iterate was already on), the exhausted ladder made ADVISORY for
# both refusals, and a ROUNDING MARGIN on the terminal polish's acceptance. The three
# E4-1..E4-26 rounds were squashed into the cherry-pick 25e11280 (of 799b527d), so the
# range this declaration gates is BEFORE..25e11280 and there is nothing to add commit by
# commit.
# This file is the previous round's gate runner (docs/notes/data/2026-09-m7-w2-nc2-flat-face/
# wrappers-gates/gates-nc2.sh), COPIED and re-pointed; that file is NOT edited. What changed,
# and nothing else:
#   E4 (a) the scratch tree, the log name and the derivation file name;
#   E4 (b) every declared value, DERIVED from THIS round's FINAL range (BEFORE = the E3 docs
#     record on m7, whose sources EQUAL NC2 fix round 1's GATED sources 0fa59cdc -- checked,
#     see psym-derivation-e4.txt) and measured by a FRESH P-SYM rehearsal on the FINAL pair,
#     both configurations; NOTHING is carried from NC2's counts;
#   E4 (c) THE CHANGED SURFACE REACHES bench/ THIS ROUND (bench/bench_scale.cpp is MODIFIED
#     and its object bench/CMakeFiles/hven_sqp_bench.dir/bench_scale.cpp.o IS in the P-SYM
#     capture universe), so CHANGED_SOURCES, MODIFIED_CXX, ADDED_CXX and the clang-format set
#     are derived over `src/ tests/ bench/` rather than `src/ tests/`. NC2's two-directory
#     derivation would have left a MODIFIED source out of the classifier's changed-source
#     list, and its object would then have been read as a class B includer whose source is
#     untouched -- which the pre-flight's own "no declared class B object may have a source
#     modified in the range" check would have refused. Derived, not carried;
#   E4 (d) NO .cpp is ADDED this round and NONE is ADDED inside the P-SYM universe; THREE
#     sources OUTSIDE the universe are MODIFIED (tests/qp/test_qp_anti_cycling_arm.cpp,
#     test_qp_degeneracy_arm.cpp, test_qp_flat_face_saddle.cpp -- all three
#     hven_qp_walk_seam_tests's, an HVEN_TESTING recompile target that is NOT a PSYM_GROUPS
#     entry; checked by value). tests/CMakeLists.txt does NOT change in this range, so the
#     carried "adds exactly the declared outside sources" check runs on an empty declaration,
#     which is the derivation NC1 ran;
#   E4 (e) THE FIXTURE RENAME EVENT (settler ruling E4-25 (a)), the round's largest declared
#     event: 24 TEST instances REMOVED and 32 ADDED by name, net +8. Twenty-four of the
#     thirty-two are RENAMES of the twenty-four removed -- the anti-cycling and degeneracy
#     arm fixtures no longer describe cycling cells, because after E4 no cell in this tree
#     still cycles -- and the eight that are not are QpE4ProximalEqp's own new cases. BOTH
#     lists are declared BY NAME below, each name checked to be added/removed by exactly ONE
#     TEST line in the range, counted with `grep -c` and never `grep -q`. The carried by-name
#     assert already had the REMOVED slot (NC2's list was empty; NC1's and the fix6 runners'
#     had the same slot); it needed no edit for a non-empty list, only a derived one;
#   E4 (f) THREE DECLARED Debug GTEST_SKIPs -- QpE2WorkingSet.AGreyPinOnYorknetThatIs
#     IndependentStaysIndependent (2682 s), QpE4ProximalEqp.TheYorknetRestorationQpIsDrift
#     FreeAndOptimal (2414 s) and NC2 fix round 1's QpNc2FlatFace.TheSteenbrbRestorationLpQp2
#     NeverAscendsTheSlopeBorder (745 s), each against M6's 600 s per-fixture budget and each
#     PASSING in Release. THAT CHANGES NO COUNT HERE, and the accounting was CHECKED rather
#     than assumed, exactly as NC2 checked its one: ctest puts a SKIPPED test INSIDE its
#     `out of N` denominator and counts it as passed (it lists it under "did not run"), and
#     only a DISABLED test is excluded -- which is why the baseline's started-name count
#     exceeds its total by exactly its two DISABLED instances. So `assert_ctest`'s two checks,
#     total == declared and failed == 0, are the right ones unchanged;
#   E4 (g) THE ADDED NON-C++ TEST FILES, DECLARED BY NAME: three dump fixtures under
#     tests/qp/fixtures/e2-d4/ (A0_YORKNET_qp3.txt, R2_Cs_STEENBRB_qp1.txt,
#     STEENBRB_def-qp2.txt; data read at run time through HVEN_QP_E2D4_FIXTURES, the outside
#     target's compile definition). The pre-flight refuses unless the derived set EQUALS the
#     list and the fixture directory is named exactly once in the build files;
#   E4 (h) G4 IS THE FIRST NON-ZERO DIFFERENCE DECLARATION SINCE U0 ITSELF (settler ruling
#     E4-25 (b)). This round declares FORTY-THREE differences against the M6 close pin --
#     walk 17, ssn 13, ipm 13 -- and the gate is "the MEASURED difference SET equals the
#     DECLARED list", compared as normalized order-free sets of (engine, cell, column), NOT
#     "differences == 0". The 1e-5 residual gate still applies and still relaxes the twelve
#     float-measure columns, so every cell NOT in the declared list must still agree within
#     it; a declared cell is declared by its COORDINATES, never by its value, because a
#     residual's last digits are not reproducible across processes (CLAUDE.md section 7).
#     The comparator's own rc is therefore 1 on every engine, which the g4 segment now
#     EXPECTS: rc 0 -- no difference at all -- is as much an undeclared shape as rc 2;
#   E4 (i) the G4 comparison label (g4 segment, one string);
#   E4 (j) the P-SYM shape this range measures. `solver_counters.h` and `qp_types.h` both
#     change, plus eqp_solve.h, bordered_eqp.h, qp_engine.h and qp_walk_trace.h, so nearly
#     every object in the capture universe moves: the class A set, the class B includers by
#     name, the configuration asymmetries, the DEBUG_ONLY counts and the library section
#     movers are all DERIVED and MEASURED on the final pair, in the slots the previous runner
#     already carries. NC2's shape (38 = 1 + 37, DEBUG_ONLY 0/6, five section movers) is NOT
#     the shape here and nothing from it is reused.
# Carried unchanged in behaviour: the by-name suite check (newline-safe), the class B list by
# name, clang-format over tests/*.h, the outside-universe check (4'), the class B asymmetry
# admission (5'), the G4 residual gate (6), the measured class A by source after G5.
#
#   G1  Debug: fresh configure, build, suite WITHOUT the >600 s test
#   G2  the >600 s test ALONE (Continuation.ProbeBudgetBoundsAFailingProposal)
#   G3  Release: fresh configure, build, suite
#   G4  the U0 27-cell three-arm replay against the M6 close pin, residual-gated, against
#       THIS ROUND'S DECLARED 43-difference SET
#   G5  P-SYM, Release and Debug, classified against $BEFORE with a DERIVED
#       changed-header list, plus the fail-closed non-debug section check
#   G6  install smoke, the export contract, clang-format, git diff --check
#
# ---- THIS ROUND'S DECLARED EXPECTATIONS -------------------------------------
# The derivation is `psym-derivation-e4.txt` (this directory), committed WITH
# this file and BEFORE the chain was detached. BEFORE = the E3 docs record on m7
# (sources identical to NC2 fix round 1's gated sources); the range is that record
# and this declaration commit (docs only) around the ONE code commit 25e11280.
# Class A from git; class B, the section shape and the test counts measured on the final
# pair.
#
# WALL IS INFORMATIONAL. Nothing this file prints is a timing claim.
# =========================================================================
set -o pipefail
ROOT=/home/ghecht/Projects/hven
S=$ROOT/.scratch/m7w2e4gates
G=$S/gates
# THE LINE ABOVE IS THE ROUND'S ONE DEFINITION OF ITS SCRATCH TREE, AND THE ONLY
# LITERAL NAMING IT ANYWHERE IN THE CHAIN. That property is M4 fix round 1's,
# bought with two refused chains, and it is carried here rather than re-earned:
#
#   * every gate SEGMENT names no path at all -- each one's second line is the
#     prologue `ROOT=...; S=${GATES_SCRATCH:?}; G=$S/gates`, so an unset export
#     is a refusal rather than a silent write somewhere else (M4 fix round 1's
#     run 1 died on six inner literals naming the previous round's tree);
#   * `assert_segment_scratch` below judges a segment by the VALUE its prologue
#     RESOLVES, not by grepping its prose -- M4 fix round 1's run 2 was refused
#     by its own guard firing on a COMMENT that used the word;
#   * and `chain_gates_e4.sh` does not repeat this path either: it ASKS for it
#     with `--print-scratch`.
export GATES_SCRATCH="$S"

# --- THE TWO NON-RUNNING MODES, ANSWERED BEFORE ANY OUTPUT IS REDIRECTED -----
# `--print-scratch` makes this file the single source of the scratch path for
# every sibling that needs it. `--preflight-only` writes and checks the six gate
# segments and stops before the first leg, so the pre-flight can be RUN and its
# output READ -- and pasted into the commit that lands it -- without spending a
# Debug suite to find out whether it passes.
PREFLIGHT_ONLY=0
case "${1:-}" in
  --print-scratch)  printf '%s\n' "$S"; exit 0 ;;
  --preflight-only) PREFLIGHT_ONLY=1; shift ;;
esac
ART=$ROOT/docs/notes/data/2026-09-m7-w2-m3b-minor-trace
M4W=$ROOT/docs/notes/data/2026-09-m7-w2-m4-anticycling-arm/wrappers
CLASSIFIER=$M4W/classify_psym_m4.py
ASSERTS=$ART/wrappers/gate_asserts_m3b.sh


# ---- THIS ROUND'S EXPECTED COUNTS -------------------------------------------
# `psym-derivation-e4.txt` §3: the by-name baseline's totals, less the 24 declared
# REMOVED instances, plus the 32 declared ADDED ones (the FIXTURE RENAME EVENT, E4 (e)).
BEFORE=${BEFORE:-84ea0385}
EXPECT_DEBUG_TESTS=${EXPECT_DEBUG_TESTS:-2869}
EXPECT_RELEASE_TESTS=${EXPECT_RELEASE_TESTS:-2870}
EXPECT_LONG_TESTS=1
EXPECT_REPLAY_CELLS=27
EXPECT_REPLAY_COLUMNS=75
EXPECT_REPLAY_ENGINES=3
# E4 (h): THE DECLARED DIFFERENCE SET, per engine and by coordinate. The FIRST NON-ZERO
# G4 declaration since U0 itself (settler ruling E4-25 (b)). The gate is NOT a count on
# its own: the MEASURED set of (engine, cell, column) triples must EQUAL the declared
# list below, order-free, and the per-engine counts must match as well -- a round that
# moved a different cell by the same number of columns fails.
# A DIFFERENCE IS DECLARED BY ITS COORDINATES, NEVER BY ITS VALUE. A residual's last
# digits are not reproducible between two processes running identical code at
# MKL_NUM_THREADS=1 (CLAUDE.md section 7), so pinning the values would pin noise. The
# 1e-5 residual gate still applies and still relaxes the twelve float-measure columns
# ONLY, so every cell NOT in this list must still agree inside it.
EXPECT_REPLAY_DIFFS_WALK=${EXPECT_REPLAY_DIFFS_WALK:-17}
EXPECT_REPLAY_DIFFS_SSN=${EXPECT_REPLAY_DIFFS_SSN:-13}
EXPECT_REPLAY_DIFFS_IPM=${EXPECT_REPLAY_DIFFS_IPM:-13}
EXPECT_REPLAY_DIFFS_TOTAL=43
# ONE `<engine> <cell> <column>` PER LINE. 43 entries: 5 integer `qp_minors` 2 -> 3 on
# the five walk `*_bound_neutral` cells, 37 residual columns strictly BETTER, and ONE
# WORSE -- `ipm f7_n1000_path_warm kkt_stationarity`, 3.449707634e-07 -> 3.449751647e-07,
# 1.3e-05 relative, which is in this list on exactly the same terms as the other 42.
DECLARED_REPLAY_DIFFS=${DECLARED_REPLAY_DIFFS-'ipm f7_n10000_bound_neutral kkt_primal
ipm f7_n10000_bound_neutral kkt_residual
ipm f7_n10000_bound_neutral kkt_stationarity
ipm f7_n1000_path_warm kkt_stationarity
ipm f7_n20000_bound_corrupted kkt_residual
ipm f7_n20000_bound_corrupted kkt_stationarity
ipm f7_n20000_bound_neutral kkt_primal
ipm f7_n20000_bound_neutral kkt_residual
ipm f7_n20000_bound_neutral kkt_stationarity
ipm f7_n2000_bound_neutral kkt_residual
ipm f7_n2000_bound_neutral kkt_stationarity
ipm f7_n5000_bound_neutral kkt_residual
ipm f7_n5000_bound_neutral kkt_stationarity
ssn f7_n10000_bound_neutral kkt_primal
ssn f7_n10000_bound_neutral kkt_residual
ssn f7_n10000_bound_neutral kkt_stationarity
ssn f7_n20000_bound_corrupted kkt_residual
ssn f7_n20000_bound_corrupted kkt_stationarity
ssn f7_n20000_bound_neutral kkt_primal
ssn f7_n20000_bound_neutral kkt_residual
ssn f7_n20000_bound_neutral kkt_stationarity
ssn f7_n2000_bound_neutral kkt_residual
ssn f7_n2000_bound_neutral kkt_stationarity
ssn f7_n5000_bound_neutral kkt_residual
ssn f7_n5000_bound_neutral kkt_stationarity
ssn f7_n800_path_warm kkt_stationarity
walk f7_n10000_bound_neutral kkt_primal
walk f7_n10000_bound_neutral kkt_residual
walk f7_n10000_bound_neutral kkt_stationarity
walk f7_n10000_bound_neutral qp_minors
walk f7_n1000_bound_neutral qp_minors
walk f7_n20000_bound_corrupted kkt_residual
walk f7_n20000_bound_corrupted kkt_stationarity
walk f7_n20000_bound_neutral kkt_primal
walk f7_n20000_bound_neutral kkt_residual
walk f7_n20000_bound_neutral kkt_stationarity
walk f7_n20000_bound_neutral qp_minors
walk f7_n2000_bound_neutral kkt_residual
walk f7_n2000_bound_neutral kkt_stationarity
walk f7_n2000_bound_neutral qp_minors
walk f7_n5000_bound_neutral kkt_residual
walk f7_n5000_bound_neutral kkt_stationarity
walk f7_n5000_bound_neutral qp_minors'}
# The one WORSE cell, named separately so the pre-flight can check it IS in the list.
DECLARED_REPLAY_WORSE='ipm f7_n1000_path_warm kkt_stationarity'
declared_diffs() { printf '%s\n' "$DECLARED_REPLAY_DIFFS" | grep -v '^$' | LC_ALL=C sort; }
# THE MEASURED SET, harvested from the comparison transcript itself. The engine comes
# from each `=== ... HEAD-<eng> vs ... ===` header line and the cell and column from the
# difference lines under it; a `WITHIN-GATE` line is a DIFFERENT LINE CLASS and is not a
# difference (compare_replay.py prints it only when the gate is given, and it never
# changes that comparator's exit status).
measured_diffs() {
  awk '
    /^=== / { eng=""; if (match($0, /HEAD-[a-z]+ vs/)) eng=substr($0, RSTART+5, RLENGTH-8); next }
    /^    WITHIN-GATE / { next }
    /^    / { if (eng != "") print eng, $1, $2 }
  ' "$1" | LC_ALL=C sort
}
# assert_replay_declared COMPARISON CELLS COLUMNS -- this round's G4 gate, in place of
# `assert_replay`'s single declared difference count, which cannot express a per-engine
# shape or a coordinate list. Everything `assert_replay` checked is still checked here:
# all three engines present, each at the pinned cells/columns shape.
assert_replay_declared() {
  local cmp="$1" cells="$2" cols="$3" rc=0 n eng want d nd
  if [ ! -s "$cmp" ]; then
    echo "ASSERT FAIL replay: $cmp is missing or empty"
    return 1
  fi
  n=$(grep -c '^=== .*cells compared:' "$cmp")
  assert_eq "replay header lines" "$n" "$EXPECT_REPLAY_ENGINES" || rc=1
  for eng in walk ssn ipm; do
    case $eng in
      walk) want=$EXPECT_REPLAY_DIFFS_WALK ;;
      ssn)  want=$EXPECT_REPLAY_DIFFS_SSN ;;
      ipm)  want=$EXPECT_REPLAY_DIFFS_IPM ;;
    esac
    n=$(grep -c "HEAD-$eng vs .*=== cells compared: ${cells}  columns compared: ${cols}  differences: ${want}  within gate: " "$cmp")
    assert_eq "replay $eng shape ${cells}/${cols}/${want} declared" "$n" 1 || rc=1
    # THE LISTED LINES ARE COUNTED TOO, not only the summary number: the comparator
    # prints at most 200 difference lines, so a count that agreed while the listing was
    # truncated would make the set comparison below read short for a reason that is not
    # the tree's.
    n=$(measured_diffs "$cmp" | grep -c "^$eng ")
    assert_eq "replay $eng difference lines listed" "$n" "$want" || rc=1
  done
  nd=$(declared_diffs | grep -c .)
  d=$(LC_ALL=C comm -3 <(declared_diffs) <(measured_diffs "$cmp"))
  if [ -z "$d" ]; then
    echo "ASSERT OK   replay: the MEASURED difference set EQUALS the DECLARED $nd-entry list"
  else
    echo "ASSERT FAIL replay: the measured difference set is not the declared one (declared '<' tab-indented '>' measured):"
    printf '%s\n' "$d" | head -60
    rc=1
  fi
  return $rc
}
# CARRIED CHANGE (6): the comparator's calibrated floating-measure gate. 1e-5 relative
# with the 1e-13 absolute floor is the suite's (tests/sqp/test_corpus_cells.cpp,
# `is_residual_column`). It relaxes the twelve float-measure columns ONLY; strings,
# statuses and the 56 integer fields stay byte-exact.
REPLAY_RESIDUAL_GATE=${REPLAY_RESIDUAL_GATE:-1e-5}
MIN_NEGATIVE_CONTROL=1

# E4 (e): THE SUITES BY NAME. The set of test names ctest STARTS (disabled ones included)
# must EQUAL the baseline transcript's LESS the declared REMOVED names PLUS the declared ADDED
# names, per configuration. THE BASELINE IS DERIVED, NOT NAMED: the newest gate record's
# suite-release.txt ADDED at or before BEFORE, its directory, and the head its suites ran at
# = the parent of the commit that added it (its chain ran at that commit; the record is
# committed on top). The pre-flight re-checks by value that no test source differs between
# that head and BEFORE.
NAMES_BASE_REL=$(git -C "$ROOT" log -1 --format= --name-only --diff-filter=A "$BEFORE" \
  -- 'docs/notes/data/*/gates-*/suite-release.txt' 'docs/notes/data/*/gates/suite-release.txt' \
  | grep -v '^$')
NAMES_BASE_DIR=$ROOT/$(dirname "$NAMES_BASE_REL")
NAMES_BASE_HEAD=$(git -C "$ROOT" rev-parse --short "$(git -C "$ROOT" log -1 --format=%H \
  --diff-filter=A "$BEFORE" -- "$NAMES_BASE_REL")^")
# ONE NAME PER LINE: a parameterised instance's ctest name carries spaces
# ("…/4-byte object <00-00 00-00>"), so the lists are newline-delimited and read line by
# line, never word-split. E4 (e), THE FIXTURE RENAME EVENT: 32 ADDED instances and 24
# REMOVED, net +8, both DERIVED by name from this round's own TEST lines and checked
# against the measured suites and the baseline (psym-derivation-e4.txt §3). Twenty-four
# of the thirty-two ADDED are the RENAMES of the twenty-four REMOVED; the remaining eight
# are QpE4ProximalEqp's new cases. NOTHING here is carried: a moved head gets a fresh
# derivation, which is exactly the error NC2's own declaration caught in its first draft.
ADDED_TEST_NAMES='QpAntiCyclingArm.ArmEIsInertOnThePeriodFourFixtureThatNoLongerCycles
QpAntiCyclingArm.ArmEIsInertOnThePeriodTwoFixtureThatNoLongerCycles
QpAntiCyclingArm.ArmLIsInertOnThePeriodFourFixtureThatNoLongerCycles
QpAntiCyclingArm.ArmLIsInertOnThePeriodTwoFixtureThatNoLongerCycles
QpAntiCyclingArm.TheOffWalkOnThePeriodFourFixtureTerminatesAtItsOptimum
QpAntiCyclingArm.TheOffWalkOnThePeriodTwoFixtureTerminatesAtItsOptimum
QpDegeneracyArm.ArmAHasNoNoiseDropToRefuseAndIsInert
QpDegeneracyArm.ArmB1HasNoPassOverToLeaveTheBoxWithAndTheCertificateIsNotReached
QpDegeneracyArm.ArmB1IsInertOnTheFixtureThatNoLongerCycles
QpDegeneracyArm.ArmB1sMuSweepFindsNoPassOverToLeakAtAnyMu
QpDegeneracyArm.TheDiagnosticColumnsStayAbsentBecauseNoDropPassRuns
QpDegeneracyArm.TheDropScaleColumnHasNoDropPassToCheck
QpDegeneracyArm.TheOffWalkOnTheScaleCarrierFixtureTakesNoNoiseDropAndTerminates
QpDegeneracyArm.ThePostDropInertiaDiagnosticHasNoDropPassToRunOnAndMovesNothing
QpE2WorkingSet.TheBorderNoLongerNeedsItsRetreatAndStillEndsOptimal
QpE2WorkingSet.TheDeclinedRideResolvesWithoutAnExchangeOnSteenbrbRestorationQp3
QpE2WorkingSet.TheNullSpaceVetoIsNotReachedOnceThePolishRemovesTheRowsFootprint
QpE2WorkingSet.TheRideNeedsNoExchangeOnceThePolishRemovesTheRowsFootprint
QpE4ProximalEqp.NoOrdinaryStepAscendsItsOwnObjectiveOnSteenbrbQp2
QpE4ProximalEqp.NoOrdinaryStepAscendsItsOwnObjectiveOnSteenbrbQp3
QpE4ProximalEqp.NoOrdinaryStepAscendsItsOwnObjectiveOnSteenbrbQp4
QpE4ProximalEqp.NoOrdinaryStepAscendsItsOwnObjectiveOnTheFlatRayLp
QpE4ProximalEqp.TheAcopp57PrimaryQpCostsTwoMinorsOnBothAlgebras
QpE4ProximalEqp.TheSteenbrbRestorationLpQp1ReachesTheClarabelOptimum
QpE4ProximalEqp.TheSteenbrbRestorationQp2ExitsFiniteAndSaysWhy
QpE4ProximalEqp.TheYorknetRestorationQpIsDriftFreeAndOptimal
QpNc2FlatFace.TheNoiseLevelMultiplierIsWithinTheScaledKktTestAndIsNotDropped
SqpDriverRadius.TheElasticExhaustionPreemptsTheRadiusFloorOnThisModel
SqpDriverRestorationSeed.TheExhaustedLadderSiteOffersItsOwnStepAndTheGuardRefusesIt
SqpDriverRestorationSeed.TheOffDiagonalStartOffersTheLadderStepAndTheGuardRefusesIt
SqpDriverRestorationSeed.TheOfferedCandidateNoLongerMeasuresBetterAndIsNotTaken
SqpDriverRestorationSeed.TheSeedOfferIsNotGatedOnTheQpModeAndIsRefusedInEveryOne'
REMOVED_TEST_NAMES='QpAntiCyclingArm.ArmEEscapesThePeriodFourCycleAndReturnsTheOptimum
QpAntiCyclingArm.ArmEEscapesThePeriodTwoCycleAndReturnsTheOptimum
QpAntiCyclingArm.ArmLEscapesThePeriodFourCycleAndReturnsTheOptimum
QpAntiCyclingArm.ArmLEscapesThePeriodTwoCycleAndReturnsTheOptimum
QpAntiCyclingArm.TheOffWalkCyclesAtPeriodFourWithExactlyZeroMotion
QpAntiCyclingArm.TheOffWalkCyclesAtPeriodTwoWithExactlyZeroMotion
QpDegeneracyArm.ArmAScaledDropRefusesTheDropAndEndsTheRecurrence
QpDegeneracyArm.ArmB1CanLeaveTheIterateOutsideItsBoxAndTheCertificateRefusesIt
QpDegeneracyArm.ArmB1EndsTheCyclingWalkNearTheOptimumButOutsideItsBox
QpDegeneracyArm.ArmB1sLeakRatioUnderTheMuSweepAmendmentA5Asks
QpDegeneracyArm.TheDiagnosticColumnsAreFilledWhereTheirBranchRan
QpDegeneracyArm.TheDropScaleIsTheSameNumberFreeBlockStationarityComputed
QpDegeneracyArm.TheOffWalkOnTheScaleCarrierFixtureReallyDoesCycle
QpDegeneracyArm.ThePostDropInertiaDiagnosticMovesNoCounterAndNoAnswer
QpE2WorkingSet.ADeclinedRideIsExchangedOnSteenbrbRestorationQp3
QpE2WorkingSet.ADependentRideBlockerIsExchangedAndTheRideRetaken
QpE2WorkingSet.ANullSpaceVetoedRideNeverStepsBackIntoTheReleasedBound
QpE2WorkingSet.TheBorderRetreatsExactlyOnceAndEndsOptimal
QpNc2FlatFace.ANoiseLevelDropIsUndoneAndCountedNotRidden
SqpDriverRadius.FloorRaisesTheRestorationRequest
SqpDriverRestorationSeed.TheExhaustedLadderSiteSeedsFromItsOwnStep
SqpDriverRestorationSeed.TheRadiusFloorSeedsFromTheRejectedTrial
SqpDriverRestorationSeed.TheSeedIsNotGatedOnTheQpMode
SqpDriverRestorationSeed.TheTakenCandidateIsRecordedOnTheRequestingRow'
# E4 (g): THE ADDED NON-C++-SOURCE TEST FILES, by name, one per line: the three dump
# fixtures (data, read at run time through HVEN_QP_E2D4_FIXTURES by the outside target
# only). `STEENBRB_def-qp2.txt` carries a hyphen where the other two carry an
# underscore; the name is the dump's own and is declared as it is on disk.
ADDED_TEST_FILES=${ADDED_TEST_FILES-'tests/qp/fixtures/e2-d4/A0_YORKNET_qp3.txt
tests/qp/fixtures/e2-d4/R2_Cs_STEENBRB_qp1.txt
tests/qp/fixtures/e2-d4/STEENBRB_def-qp2.txt'}
suite_names() { sed -nE 's/^ +Start +[0-9]+: (.*[^ ]) *$/\1/p' "$1" | grep -v '^$' | LC_ALL=C sort; }
added_names() { printf '%s\n' "$ADDED_TEST_NAMES" | grep -v '^$' | LC_ALL=C sort; }
removed_names() { printf '%s\n' "$REMOVED_TEST_NAMES" | grep -v '^$' | LC_ALL=C sort; }
assert_names() {
  local label="$1" got="$2" base="$3" ng nb na nr n_in_base n_in_got d
  [ -s "$got" ] && [ -s "$base" ] || { echo "ASSERT FAIL $label by name: a transcript is missing ($got, $base)"; return 1; }
  na=$(added_names | grep -c .); nr=$(removed_names | grep -c .)
  # Counts, never `grep -q` under pipefail: every declared ADDED name is ABSENT from
  # the baseline and PRESENT exactly once in this run; every declared REMOVED name is
  # PRESENT exactly once in the baseline and ABSENT from this run.
  while IFS= read -r t; do
    n_in_base=$(suite_names "$base" | grep -cxF -- "$t"); n_in_got=$(suite_names "$got" | grep -cxF -- "$t")
    if [ "$n_in_base" != 0 ] || [ "$n_in_got" != 1 ]; then
      echo "ASSERT FAIL $label by name: ADDED $t is named $n_in_base time(s) in the baseline (want 0) and $n_in_got in this run (want 1)"; return 1
    fi
  done < <(added_names)
  while IFS= read -r t; do
    n_in_base=$(suite_names "$base" | grep -cxF -- "$t"); n_in_got=$(suite_names "$got" | grep -cxF -- "$t")
    if [ "$n_in_base" != 1 ] || [ "$n_in_got" != 0 ]; then
      echo "ASSERT FAIL $label by name: REMOVED $t is named $n_in_base time(s) in the baseline (want 1) and $n_in_got in this run (want 0)"; return 1
    fi
  done < <(removed_names)
  ng=$(suite_names "$got" | grep -c .); nb=$(suite_names "$base" | grep -c .)
  d=$(LC_ALL=C comm -3 <( (suite_names "$base" | LC_ALL=C comm -23 - <(removed_names); added_names) | LC_ALL=C sort) <(suite_names "$got"))
  if [ "$ng" -gt 0 ] && [ "$ng" = "$((nb - nr + na))" ] && [ -z "$d" ]; then
    echo "ASSERT OK   $label by name: $ng started names == the baseline's $nb - the $nr declared REMOVED + the $na declared ADDED"
  else
    echo "ASSERT FAIL $label by name: $ng started vs the baseline's $nb - $nr REMOVED + $na ADDED; the difference (expected '<' tab-indented '>' this run):"
    printf '%s\n' "$d" | head -20
    return 1
  fi
}
# OFFLINE, no scratch tree, no log, no leg -- the full by-name check of LOG against
# the declared baseline for CFG (debug|release). rc 0 pass, rc 2 refused.
if [ "${1:-}" = --check-names ]; then
  case "${2:-}" in debug|release) : ;; *) echo "usage: --check-names debug|release LOG"; exit 2 ;; esac
  assert_names "offline ${2}" "${3:?LOG}" "$NAMES_BASE_DIR/suite-${2}.txt" && exit 0
  exit 2
fi
# OFFLINE, no scratch tree, no log, no leg -- E4 (h)'s own G4 gate run against an ALREADY
# WRITTEN comparison transcript, so the declared 43-entry set and the assertion that compares
# it can be PROVED against a real transcript before a single leg is paid for. It is the same
# facility `--check-names` is, for the round's other large declared event, and it is what let
# the declared list be checked against the implementer's own U0 evidence at declaration time.
# rc 0 pass, rc 2 refused.
if [ "${1:-}" = --check-g4 ]; then
  . "$ASSERTS" || exit 90
  assert_replay_declared "${2:?COMPARISON}" "$EXPECT_REPLAY_CELLS" "$EXPECT_REPLAY_COLUMNS" && exit 0
  exit 2
fi

# ---- THIS ROUND'S EXPECTED P-SYM MOVER SHAPE, PER CONFIGURATION ----------------
# `psym-derivation-e4.txt` §2. Written RELEASE/DEBUG; g5.sh splits them.
# MEASURED ON THE FINAL PAIR -- BEFORE against the fix-round-3 head 0d519a2b -- by a fresh
# rehearsal of THIS runner's own g5 segment, both configurations, one hold of the box lock
# (rehearsal/rehearsal-log-summary.txt), and by the offline re-classification of its compare
# files at the measured differing counts, which is what fixes the A/B split and the class B
# names below (rehearsal/rehearsal-offline-log.txt, rc 0 in BOTH configurations). NOTHING is
# carried from NC2's 38 = 1 + 37: that range changed two headers, this one changes six.
#   Release  DIFFERING 69 = class A 30 + class B 39; UNEXPLAINED 0; negative control 78
#   Debug    DIFFERING 70 = class A 30 + class B 40; UNEXPLAINED 0; negative control 61
EXPECT_PSYM_DIFFERING=${EXPECT_PSYM_DIFFERING:-69/70}
EXPECT_PSYM_CLASS_A=${EXPECT_PSYM_CLASS_A:-30/30}
EXPECT_PSYM_CLASS_B=${EXPECT_PSYM_CLASS_B:-39/40}
# THE CLASS B OBJECTS BY NAME, per configuration -- BOTH MEASURED on the final pair, not one
# measured and one inferred. One cause for all of them: every one includes solver_counters.h or
# qp_types.h (directly or transitively) and none has a source modified in the range (checked,
# object by object, in the pre-flight). The two sets differ by EXACTLY ONE object, declared in
# the asymmetry slot below.
EXPECT_CLASS_B_NAMES_RELEASE=${EXPECT_CLASS_B_NAMES_RELEASE-"CMakeFiles/hven.dir/src/core/ledger.cpp.o CMakeFiles/hven.dir/src/drivers/sqp_print.cpp.o CMakeFiles/hven.dir/src/drivers/trace_writer.cpp.o CMakeFiles/hven.dir/src/globalization/sqp/soc_elastic_restoration.cpp.o CMakeFiles/hven.dir/src/qp/ipqp_engine.cpp.o CMakeFiles/hven.dir/src/warmstart/continuation.cpp.o bench/CMakeFiles/hven_sqp_corpus.dir/bench_corpus.cpp.o bench/CMakeFiles/hven_sqp_crossover.dir/bench_crossover.cpp.o bench/CMakeFiles/hven_sqp_f7_cold.dir/bench_f7_cold.cpp.o bench/CMakeFiles/hven_sqp_ipqp_e1_arm.dir/ipqp_e1_arm.cpp.o bench/CMakeFiles/hven_sqp_ssn_safeguard_probe.dir/ssn_safeguard_probe.cpp.o bench/CMakeFiles/hven_sqp_tau_bar_sweep_probe.dir/tau_bar_sweep_probe.cpp.o tests/drivers/CMakeFiles/hven_drivers_tests.dir/test_concurrency.cpp.o tests/drivers/CMakeFiles/hven_drivers_tests.dir/test_console_sink.cpp.o tests/drivers/CMakeFiles/hven_drivers_tests.dir/test_options.cpp.o tests/drivers/CMakeFiles/hven_drivers_tests.dir/test_solve_result.cpp.o tests/drivers/CMakeFiles/hven_drivers_tests.dir/test_warm_protocol.cpp.o tests/interior/CMakeFiles/hven_interior_tests.dir/Unity/unity_1_cxx.cxx.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_activity_telemetry.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_assembly_eval_seam.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_continuation.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_crossover_legs.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_dense_oracle.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_elastic_rung_vehicle.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_ipqp_counters.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_ipqp_engine.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_ipqp_trace.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_ipqp_warm_restart.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_ledger.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_mesh_transfer.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_nlp_kkt_check.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_parametric_families.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_problem_scaling.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_qp_cap_levers.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_qp_engine_tr.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_qp_warm_start.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_sqp_warm_currency.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_ssn_engine.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_start_point_projection.cpp.o"}
EXPECT_CLASS_B_NAMES_DEBUG=${EXPECT_CLASS_B_NAMES_DEBUG-"CMakeFiles/hven.dir/src/core/ledger.cpp.o CMakeFiles/hven.dir/src/drivers/sqp_print.cpp.o CMakeFiles/hven.dir/src/drivers/trace_writer.cpp.o CMakeFiles/hven.dir/src/globalization/sqp/soc_elastic_restoration.cpp.o CMakeFiles/hven.dir/src/qp/ipqp_engine.cpp.o CMakeFiles/hven.dir/src/warmstart/continuation.cpp.o bench/CMakeFiles/hven_sqp_corpus.dir/bench_corpus.cpp.o bench/CMakeFiles/hven_sqp_crossover.dir/bench_crossover.cpp.o bench/CMakeFiles/hven_sqp_f7_cold.dir/bench_f7_cold.cpp.o bench/CMakeFiles/hven_sqp_ipqp_e1_arm.dir/ipqp_e1_arm.cpp.o bench/CMakeFiles/hven_sqp_ssn_safeguard_probe.dir/ssn_safeguard_probe.cpp.o bench/CMakeFiles/hven_sqp_tau_bar_sweep_probe.dir/tau_bar_sweep_probe.cpp.o tests/drivers/CMakeFiles/hven_drivers_tests.dir/test_concurrency.cpp.o tests/drivers/CMakeFiles/hven_drivers_tests.dir/test_console_sink.cpp.o tests/drivers/CMakeFiles/hven_drivers_tests.dir/test_options.cpp.o tests/drivers/CMakeFiles/hven_drivers_tests.dir/test_solve_result.cpp.o tests/drivers/CMakeFiles/hven_drivers_tests.dir/test_solve_status.cpp.o tests/drivers/CMakeFiles/hven_drivers_tests.dir/test_warm_protocol.cpp.o tests/interior/CMakeFiles/hven_interior_tests.dir/Unity/unity_1_cxx.cxx.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_activity_telemetry.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_assembly_eval_seam.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_continuation.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_crossover_legs.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_dense_oracle.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_elastic_rung_vehicle.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_ipqp_counters.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_ipqp_engine.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_ipqp_trace.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_ipqp_warm_restart.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_ledger.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_mesh_transfer.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_nlp_kkt_check.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_parametric_families.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_problem_scaling.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_qp_cap_levers.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_qp_engine_tr.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_qp_warm_start.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_sqp_warm_currency.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_ssn_engine.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_start_point_projection.cpp.o"}
# THE CONFIGURATION-ASYMMETRIC CLASS A, DECLARED BY NAME: EMPTY -- the two class A sets are
# IDENTICAL object for object (MEASURED, `cmp` on the two sorted lists), 30 in each, and every
# one of the 30 is an in-universe MODIFIED .cpp. The slot and its checks are carried with an
# empty list.
EXPECT_CLASS_A_NOISE_ONLY_DEBUG=${EXPECT_CLASS_A_NOISE_ONLY_DEBUG-""}
# CARRIED CHANGE (5'), AND THE ONE PLACE THIS ROUND HAD TO WIDEN IT -- the
# configuration-asymmetric class B, DECLARED BY NAME: ONE object,
#   tests/drivers/CMakeFiles/hven_drivers_tests.dir/test_solve_status.cpp.o
# which is class B in Debug and NOT A MOVER AT ALL in Release. MEASURED: it is BYTE-IDENTICAL in
# Release -- it carries no verdict line in the Release compare and it is named in the Release
# NEGATIVE CONTROL -- while in Debug it DIFFERS, on exactly one symbol,
# `RestorationSubStatus_TheMapIsTotalAndTheSpellingsAreSolveStatusSPlusNotRun_Test::TestBody()`,
# whose normalized listing is 1363 lines before and 1370 after. INFERRED cause: that body
# enumerates the status spellings this range's solver_counters.h touches, and at -O0 the
# enumeration emits seven more instructions where Release folds it to identical bytes.
#
# THE E2 ROUNDS' (5') ADMISSION REQUIRED THE DECLARED OBJECT TO BE `NOISE-ONLY` IN RELEASE, and
# that is a NARROWER condition than the fact: NOISE-ONLY means "its Release bytes moved, by
# __LINE__-class noise only", and this object's Release bytes did not move at all. The carried
# check would have REFUSED a correct measurement, so the admission is widened HERE, in this
# copy, to "NOT A MOVER in Release -- NOISE-ONLY or in the NEGATIVE CONTROL -- with which of the
# two named per object". WHAT IT DOES NOT WEAKEN: the object is still declared BY NAME, the set
# difference must still be EXACTLY the declared list with nothing Release-only, the object must
# still be POSITIVELY found in one of Release's two non-mover classes (absence is not accepted),
# the negative-control listing must be COMPLETE for that reading to be sound (the classifier
# prints at most 80 and Release's control is 78 -- checked by value, not assumed), the
# cross-config copy must still remove exactly the declared lines, and UNEXPLAINED must still be
# 0 in both. The same physics the gate-assert library already admits for class A -- a TU may
# compile identically in one configuration and not in the other -- is what is being admitted
# for class B; the library's own comment makes that argument for class A in exactly these terms.
EXPECT_PSYM_B_DEBUG_ONLY=${EXPECT_PSYM_B_DEBUG_ONLY-"tests/drivers/CMakeFiles/hven_drivers_tests.dir/test_solve_status.cpp.o"}
# E4 (d): NO .cpp is ADDED against BEFORE at all (derived from git; declared count 0),
# so nothing is ADDED inside the P-SYM universe either and the ADDED-outside list is empty.
EXPECT_ADDED_CXX=${EXPECT_ADDED_CXX:-0}
EXPECT_ADDED_OUTSIDE_UNIVERSE=${EXPECT_ADDED_OUTSIDE_UNIVERSE-""}
EXPECT_PSYM_ADDED=${EXPECT_PSYM_ADDED:-0}
# The outside target: hven_qp_walk_seam_tests (an HVEN_TESTING recompile target, not a
# PSYM_GROUPS entry; checked by value). E4 (d): THREE of its sources are MODIFIED this
# round and none is ADDED, so it is the MODIFIED-outside slot that carries a list here and
# the ADDED-outside slot that is empty -- the mirror image of NC2.
OUTSIDE_TARGET=hven_qp_walk_seam_tests
OUTSIDE_DIR=tests/CMakeFiles/$OUTSIDE_TARGET.dir
EXPECT_CLASS_A_OUTSIDE_SRCS=${EXPECT_CLASS_A_OUTSIDE_SRCS-"tests/qp/test_qp_anti_cycling_arm.cpp tests/qp/test_qp_degeneracy_arm.cpp tests/qp/test_qp_flat_face_saddle.cpp"}
EXPECT_CLASS_A_OUTSIDE_UNIVERSE=$(printf '%s' "$EXPECT_CLASS_A_OUTSIDE_SRCS" | wc -w)

# ---- THE DECLARED SECTION SHAPE, PER CONFIGURATION --------------------------
# MEASURED by the same rehearsal (rehearsal/rehearsal-sections-{Release,Debug}.txt):
#   Release  44 objects, 35 byte-identical, DEBUG_ONLY 0, NON-DEBUG MOVERS 9, REFUSED 0
#   Debug    44 objects, 30 byte-identical, DEBUG_ONLY 5, NON-DEBUG MOVERS 9, REFUSED 0
# The NINE non-debug movers are THE SAME NINE in both configurations (OBSERVED, `diff` on the
# two sorted NON_DEBUG_MOVERS_GOT lists): the three DERIVED class A library objects
# (drivers/sqp_solver, qp/qp_engine, qp/ws_independence) plus the SIX declared class B ones
# below, each of which is checked unmodified in the range and named in its configuration's class
# B list before the legs. The NOISE-ONLY library-mover slot is EMPTY in both configurations --
# every mover here moved in a real section, not by noise.
EXPECT_LIB_NOISE_MOVERS_RELEASE=${EXPECT_LIB_NOISE_MOVERS_RELEASE-""}
EXPECT_LIB_CLASS_B_RELEASE=${EXPECT_LIB_CLASS_B_RELEASE-"CMakeFiles/hven.dir/src/core/ledger.cpp.o CMakeFiles/hven.dir/src/drivers/sqp_print.cpp.o CMakeFiles/hven.dir/src/drivers/trace_writer.cpp.o CMakeFiles/hven.dir/src/globalization/sqp/soc_elastic_restoration.cpp.o CMakeFiles/hven.dir/src/qp/ipqp_engine.cpp.o CMakeFiles/hven.dir/src/warmstart/continuation.cpp.o"}
EXPECT_LIB_CLASS_B_DEBUG=${EXPECT_LIB_CLASS_B_DEBUG-"CMakeFiles/hven.dir/src/core/ledger.cpp.o CMakeFiles/hven.dir/src/drivers/sqp_print.cpp.o CMakeFiles/hven.dir/src/drivers/trace_writer.cpp.o CMakeFiles/hven.dir/src/globalization/sqp/soc_elastic_restoration.cpp.o CMakeFiles/hven.dir/src/qp/ipqp_engine.cpp.o CMakeFiles/hven.dir/src/warmstart/continuation.cpp.o"}
EXPECT_LIB_NOISE_MOVERS_DEBUG=${EXPECT_LIB_NOISE_MOVERS_DEBUG-""}
# DEBUG_ONLY: Release 0, Debug 5 -- drivers/assembly_eval_seam, drivers/console_trace_sink,
# drivers/ipm_solver, drivers/sqp_options, qp/ssn_engine. INFERRED cause: the debug line tables
# of the six changed headers. NC2's Debug 6 is a different set and is not carried.
EXPECT_DEBUG_ONLY_RELEASE=${EXPECT_DEBUG_ONLY_RELEASE:-0}
EXPECT_DEBUG_ONLY_DEBUG=${EXPECT_DEBUG_ONLY_DEBUG:-5}

mkdir -p "$G" "$S/logs"
LOG=$S/logs/gates-e4.log
# THE PRE-FLIGHT SPEAKS TO THE TERMINAL. A real run's transcript is the log; a
# `--preflight-only` run exists to be READ by the person about to commit it, so
# it is not redirected and it does not overwrite the previous run's log.
[ "$PREFLIGHT_ONLY" -eq 1 ] || exec >"$LOG" 2>&1
echo "GATES_START $(date -Is)"
cd "$ROOT" || exit 90
echo "=== HEAD $(git rev-parse --short HEAD)  dirty-files=$(git status --porcelain | wc -l) ==="
# THE CLEAN-TREE REFUSAL IS THE RUN'S, NOT THE PRE-FLIGHT'S. A pre-flight is run
# precisely when the repair it checks is still uncommitted, so refusing on a
# dirty tree there would make the check unrunnable exactly when it is needed.
# The real chain still refuses, and says so here so the exemption is not silent.
if [ "$PREFLIGHT_ONLY" -eq 1 ]; then
  echo "=== PREFLIGHT_ONLY: the clean-tree refusal is NOT applied in this mode; the chain still applies it ==="
else
  [ -z "$(git status --porcelain)" ] || { echo "REFUSED: the tree is not clean"; exit 2; }
fi
echo "=== GATES_SCRATCH=$GATES_SCRATCH ==="
echo "=== BEFORE=$BEFORE  EXPECT_DEBUG_TESTS=$EXPECT_DEBUG_TESTS  EXPECT_RELEASE_TESTS=$EXPECT_RELEASE_TESTS ==="

# ---- THE UNMEASURED-SLOT REFUSAL -------------------------------------------
# A P-SYM slot not yet MEASURED on this round's FINAL pair carries the literal sentinel
# `UNMEASURED`, and this runner refuses on it here -- before any leg, before any build tree is
# touched, and in `--preflight-only` mode too. A declaration nobody measured is not a
# declaration, and the failure mode this closes is the one that has cost this window three
# refused chains: a value CARRIED from the previous round because it was already in the slot.
# THE SENTINELS ARE GONE AS OF THE FIX-ROUND-3 REHEARSAL -- every slot below now holds a number
# or a name list measured on 84ea0385..0d519a2b -- so this loop passes; it is kept because it is
# what makes the next copy of this runner refuse until ITS pair has been rehearsed, and because
# the fix-3 head moved the pair once already.
for _v in EXPECT_PSYM_DIFFERING EXPECT_PSYM_CLASS_A EXPECT_PSYM_CLASS_B \
          EXPECT_CLASS_B_NAMES_RELEASE EXPECT_CLASS_B_NAMES_DEBUG \
          EXPECT_LIB_CLASS_B_RELEASE EXPECT_LIB_CLASS_B_DEBUG \
          EXPECT_DEBUG_ONLY_RELEASE EXPECT_DEBUG_ONLY_DEBUG; do
  case "${!_v}" in
    *UNMEASURED*)
      echo "REFUSED: $_v still carries the UNMEASURED sentinel -- the FINAL pair has not been"
      echo "         rehearsed, so this round has no measured P-SYM shape to gate against."
      exit 2 ;;
  esac
done
echo "=== UNMEASURED_SLOTS_OK: every P-SYM slot carries a value ==="

# THE ROUND'S CHANGED SURFACE, DERIVED FROM GIT rather than typed: the header
# list is what the classifier checks its own declared list against, and the
# source list is what class A is keyed on.
HEAD_AT_START=$(git rev-parse HEAD)
echo "=== HEAD_AT_START=$HEAD_AT_START"
CHANGED_HEADERS=$(git diff --name-only "$BEFORE..$HEAD_AT_START" -- include/ | tr '\n' ' ')
# E4 (c): bench/ IS IN THE DERIVATION. bench/bench_scale.cpp is MODIFIED in this range
# and its object IS in the P-SYM capture universe (PSYM_GROUPS names hven_sqp_bench), so
# leaving bench/ out would hand the classifier a changed-source list that does not name a
# changed source, and its object would be read as a class B includer whose source is
# untouched. NC2's range touched no bench source, so its two-directory derivation was
# right there and is wrong here.
CHANGED_SOURCES=$(git diff --name-only "$BEFORE..$HEAD_AT_START" -- src/ tests/ bench/ | tr '\n' ' ')
echo "=== CHANGED HEADERS: $CHANGED_HEADERS"
echo "=== CHANGED SOURCES: $CHANGED_SOURCES"
[ -n "$CHANGED_HEADERS" ] || { echo "REFUSED: no changed header under include/ against $BEFORE"; exit 2; }
[ -n "$CHANGED_SOURCES" ] || { echo "REFUSED: no changed source against $BEFORE"; exit 2; }

# THE MODIFIED AND THE ADDED SOURCES, DERIVED SEPARATELY. A MODIFIED source's
# object is a class A candidate; an ADDED source's object has no before-arm
# counterpart and is asserted ONLY-AFTER in G5.
MODIFIED_CXX=$(git diff --name-only --diff-filter=M "$BEFORE..$HEAD_AT_START" -- 'src/*.cpp' 'tests/*.cpp' 'bench/*.cpp' | tr '\n' ' ')
ADDED_CXX=$(git diff --name-only --diff-filter=A "$BEFORE..$HEAD_AT_START" -- 'src/*.cpp' 'tests/*.cpp' 'bench/*.cpp' | tr '\n' ' ')
echo "=== MODIFIED C++ SOURCES: $MODIFIED_CXX"
echo "=== ADDED C++ SOURCES: $ADDED_CXX"
n_mod=$(printf '%s' "$MODIFIED_CXX" | wc -w)
n_add=$(printf '%s' "$ADDED_CXX" | wc -w)
# VALUE-BASED PRE-FLIGHT, three checks, before any leg: the derived class A
# count, the derived ADDED count, and the derived library-mover list against
# what this round declares.
# VALUE-BASED PRE-FLIGHT, before any leg: the derived class A count, the
# derived ADDED count, the outside-universe sources, the derived library-mover
# list and the G4 declaration, against what this round declares.
want_a_rel=${EXPECT_PSYM_CLASS_A%/*}; want_a_dbg=${EXPECT_PSYM_CLASS_A#*/}
n_a_noise_dbg=$(printf '%s' "$EXPECT_CLASS_A_NOISE_ONLY_DEBUG" | wc -w)
if [ "$((n_mod - EXPECT_CLASS_A_OUTSIDE_UNIVERSE))" != "$want_a_rel" ] || [ "$want_a_dbg" != "$((want_a_rel - n_a_noise_dbg))" ]; then
  echo "REFUSED: $n_mod modified .cpp (less $EXPECT_CLASS_A_OUTSIDE_UNIVERSE outside the universe; Debug less $n_a_noise_dbg declared NOISE-ONLY), but class A is DECLARED $EXPECT_PSYM_CLASS_A"
  exit 2
fi
if [ "$n_add" != "$EXPECT_ADDED_CXX" ]; then
  echo "REFUSED: $n_add added .cpp derived, but ADDED is DECLARED $EXPECT_ADDED_CXX"
  exit 2
fi
# CARRIED CHANGE (4'): every declared outside source is MODIFIED, is a source of the
# outside target in tests/CMakeLists.txt, is named EXACTLY ONCE in the build
# files (so its only object is the outside one), and that target is NOT a
# P-SYM group. Value checks, not prose.
outside_block=$(awk -v t="$OUTSIDE_TARGET" '$0 ~ "add_executable\\("t"([[:space:]]|$)" {f=1} f {print} f && /\)/ {exit}' tests/CMakeLists.txt)
[ -n "$outside_block" ] || { echo "REFUSED: no add_executable($OUTSIDE_TARGET in tests/CMakeLists.txt"; exit 2; }
# The same checks over the ADDED outside sources, which must be ADDED (not modified). The
# list is EMPTY this round, so this loop does not execute; the MODIFIED-outside loop below
# is the one that carries E4's three seam sources.
for f in $EXPECT_ADDED_OUTSIDE_UNIVERSE; do
  printf '%s\n' $ADDED_CXX | grep -qxF "$f" || { echo "REFUSED: declared outside source $f is not ADDED against $BEFORE"; exit 2; }
  printf '%s\n' "$outside_block" | grep -qF "${f#tests/}" || { echo "REFUSED: $f is not a source of $OUTSIDE_TARGET in tests/CMakeLists.txt"; exit 2; }
  n_all=$(git grep -hF "$(basename "$f")" -- 'CMakeLists.txt' '*/CMakeLists.txt' '*.cmake' | wc -l)
  n_blk=$(printf '%s\n' "$outside_block" | grep -cF "$(basename "$f")")
  [ "$n_all" = 1 ] && [ "$n_blk" = 1 ] \
    || { echo "REFUSED: $f is named $n_all time(s) in the build files, $n_blk in $OUTSIDE_TARGET's block -- its object may be captured elsewhere"; exit 2; }
done
for f in $EXPECT_CLASS_A_OUTSIDE_SRCS; do
  printf '%s\n' $MODIFIED_CXX | grep -qxF "$f" || { echo "REFUSED: declared outside source $f is not MODIFIED against $BEFORE"; exit 2; }
  printf '%s\n' "$outside_block" | grep -qF "${f#tests/}" || { echo "REFUSED: $f is not a source of $OUTSIDE_TARGET in tests/CMakeLists.txt"; exit 2; }
  n_all=$(git grep -hF "$(basename "$f")" -- 'CMakeLists.txt' '*/CMakeLists.txt' '*.cmake' | wc -l)
  n_blk=$(printf '%s\n' "$outside_block" | grep -cF "$(basename "$f")")
  [ "$n_all" = 1 ] && [ "$n_blk" = 1 ] \
    || { echo "REFUSED: $f is named $n_all time(s) in the build files, $n_blk in $OUTSIDE_TARGET's block -- its object may be captured elsewhere"; exit 2; }
done
if sed -n '/^PSYM_GROUPS=(/,/^)/p' scripts/psym_compare.sh | grep -qF "$OUTSIDE_DIR"; then
  echo "REFUSED: $OUTSIDE_DIR IS a P-SYM group -- its objects would be captured"; exit 2
fi
sed -n '/^PSYM_GROUPS=(/,/^)/p' scripts/psym_compare.sh | grep -q 'CMakeFiles' \
  || { echo "REFUSED: could not read PSYM_GROUPS from scripts/psym_compare.sh"; exit 2; }
# The ADDED sources inside the universe are the ADDED ones less the declared outside ones.
# Both are empty this round, and the derivation is still taken rather than short-circuited.
ADDED_IN_UNIVERSE=$(printf '%s\n' $ADDED_CXX | grep -vxF -f <(printf '%s\n' $EXPECT_ADDED_OUTSIDE_UNIVERSE) | tr '\n' ' ')
n_add_u=$(printf '%s' "$ADDED_IN_UNIVERSE" | wc -w)
echo "=== MODIFIED OUTSIDE THE P-SYM UNIVERSE (declared, checked): [$EXPECT_CLASS_A_OUTSIDE_SRCS] -> $OUTSIDE_DIR"
echo "=== ADDED OUTSIDE THE P-SYM UNIVERSE (declared, checked): [$EXPECT_ADDED_OUTSIDE_UNIVERSE] -> $OUTSIDE_DIR"
echo "=== ADDED INSIDE THE P-SYM UNIVERSE (derived): $ADDED_IN_UNIVERSE"
if [ "$n_add_u" != "$EXPECT_PSYM_ADDED" ]; then
  echo "REFUSED: $n_add_u added .cpp inside the universe, but the universe's ADDED is DECLARED $EXPECT_PSYM_ADDED"
  exit 2
fi
lib_a=$(git diff --name-only --diff-filter=M "$BEFORE..$HEAD_AT_START" -- 'src/*.cpp' \
  | sed 's|^|CMakeFiles/hven.dir/|; s|$|.o|')
EXPECT_LIB_ADDED=$(git diff --name-only --diff-filter=A "$BEFORE..$HEAD_AT_START" -- 'src/*.cpp' \
  | sed 's|^|CMakeFiles/hven.dir/|; s|$|.o|' | LC_ALL=C sort | tr '\n' ' ')
EXPECT_LIB_MOVERS_RELEASE=$(printf '%s\n' $lib_a $EXPECT_LIB_NOISE_MOVERS_RELEASE $EXPECT_LIB_CLASS_B_RELEASE | LC_ALL=C sort -u | tr '\n' ' ')
EXPECT_LIB_MOVERS_DEBUG=$(printf '%s\n' $lib_a $EXPECT_LIB_NOISE_MOVERS_DEBUG $EXPECT_LIB_CLASS_B_DEBUG | LC_ALL=C sort -u | tr '\n' ' ')
echo "=== DECLARED NON-DEBUG MOVERS Release (derived class A + declared NOISE-ONLY + declared class B): $EXPECT_LIB_MOVERS_RELEASE"
echo "=== DECLARED NON-DEBUG MOVERS Debug   (derived class A + declared NOISE-ONLY + declared class B): $EXPECT_LIB_MOVERS_DEBUG"
echo "=== DECLARED ADDED LIBRARY OBJECTS (derived): $EXPECT_LIB_ADDED"
for o in $EXPECT_LIB_NOISE_MOVERS_RELEASE $EXPECT_LIB_NOISE_MOVERS_DEBUG; do
  src=${o#CMakeFiles/hven.dir/}; src=${src%.o}
  if printf '%s\n' $lib_a | grep -qxF "$o"; then
    echo "REFUSED: $o is declared a NOISE-ONLY library mover but its source $src is MODIFIED (class A)"; exit 2
  fi
  [ -f "$src" ] || { echo "REFUSED: declared class B $o has no source $src"; exit 2; }
done
# THE DECLARED COUNTS ARE CONSISTENT WITH EACH OTHER, by value: per
# configuration DIFFERING = class A + class B; the Debug suite is the Release
# suite less the long test(s) G2 runs alone; the DEBUG_ONLY counts are integers.
for cfg in rel dbg; do
  if [ $cfg = rel ]; then d=${EXPECT_PSYM_DIFFERING%/*}; a=${EXPECT_PSYM_CLASS_A%/*}; b=${EXPECT_PSYM_CLASS_B%/*}
  else d=${EXPECT_PSYM_DIFFERING#*/}; a=${EXPECT_PSYM_CLASS_A#*/}; b=${EXPECT_PSYM_CLASS_B#*/}; fi
  [ "$d" = "$((a + b))" ] || { echo "REFUSED: $cfg DIFFERING $d != class A $a + class B $b"; exit 2; }
done
[ "$EXPECT_DEBUG_TESTS" = "$((EXPECT_RELEASE_TESTS - EXPECT_LONG_TESTS))" ] \
  || { echo "REFUSED: Debug $EXPECT_DEBUG_TESTS != Release $EXPECT_RELEASE_TESTS - long $EXPECT_LONG_TESTS"; exit 2; }
case "$EXPECT_DEBUG_ONLY_RELEASE$EXPECT_DEBUG_ONLY_DEBUG" in *[!0-9]*|"") echo "REFUSED: DEBUG_ONLY counts not integers"; exit 2 ;; esac
# CARRIED CHANGE (6), by value: the gate is exactly the calibrated 1e-5, and the
# comparator at HEAD implements the flag and the twelve-column set.
[ "$REPLAY_RESIDUAL_GATE" = "1e-5" ] || { echo "REFUSED: G4's residual gate is '$REPLAY_RESIDUAL_GATE', declared 1e-5"; exit 2; }
grep -qF 'FLOAT_MEASURE_COLUMNS = frozenset([12, 15, 16, 17, 18, 19, 20, 42, 43, 55, 72, 73])' scripts/compare_replay.py \
  || { echo "REFUSED: scripts/compare_replay.py does not carry the twelve-column float-measure set"; exit 2; }
grep -qF 'arg == "--residual-gate"' scripts/compare_replay.py \
  || { echo "REFUSED: scripts/compare_replay.py does not take --residual-gate"; exit 2; }
# E4 (h), by value: the declared list is well formed BEFORE any leg -- 43 entries with no
# duplicate, the per-engine counts summing to it, every engine one of the three, every cell
# one of the 27 U0 cells, and the one declared WORSE cell present in it.
n_g4=$(declared_diffs | grep -c .); n_g4u=$(declared_diffs | uniq | grep -c .)
[ "$n_g4" = "$EXPECT_REPLAY_DIFFS_TOTAL" ] && [ "$n_g4" = "$n_g4u" ] \
  || { echo "REFUSED: the declared G4 list has $n_g4 entr(ies) ($n_g4u unique), declared $EXPECT_REPLAY_DIFFS_TOTAL"; exit 2; }
[ "$EXPECT_REPLAY_DIFFS_TOTAL" = "$((EXPECT_REPLAY_DIFFS_WALK + EXPECT_REPLAY_DIFFS_SSN + EXPECT_REPLAY_DIFFS_IPM))" ] \
  || { echo "REFUSED: the declared G4 total $EXPECT_REPLAY_DIFFS_TOTAL != walk $EXPECT_REPLAY_DIFFS_WALK + ssn $EXPECT_REPLAY_DIFFS_SSN + ipm $EXPECT_REPLAY_DIFFS_IPM"; exit 2; }
for eng in walk ssn ipm; do
  case $eng in walk) w=$EXPECT_REPLAY_DIFFS_WALK ;; ssn) w=$EXPECT_REPLAY_DIFFS_SSN ;; ipm) w=$EXPECT_REPLAY_DIFFS_IPM ;; esac
  n=$(declared_diffs | grep -c "^$eng ")
  [ "$n" = "$w" ] || { echo "REFUSED: the declared G4 list names $n $eng entr(ies), declared $w"; exit 2; }
done
n_g4bad=$(declared_diffs | grep -cvE '^(walk|ssn|ipm) [A-Za-z0-9_]+ [A-Za-z0-9_]+$')
[ "$n_g4bad" = 0 ] || { echo "REFUSED: $n_g4bad declared G4 entr(ies) are not '<engine> <cell> <column>'"; exit 2; }
U0CELLS=$(grep -v '^#' "$ROOT/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt" | tr -d '\n' | tr ',' '\n' | grep -v '^$' | LC_ALL=C sort -u)
n_u0=$(printf '%s\n' "$U0CELLS" | grep -c .)
[ "$n_u0" = "$EXPECT_REPLAY_CELLS" ] || { echo "REFUSED: the U0 cell list names $n_u0 cell(s), declared $EXPECT_REPLAY_CELLS"; exit 2; }
n_g4cell=$(declared_diffs | awk '{print $2}' | LC_ALL=C sort -u | grep -vxF -f <(printf '%s\n' "$U0CELLS") | grep -c .)
[ "$n_g4cell" = 0 ] || { echo "REFUSED: $n_g4cell declared G4 cell(s) are not U0 replay cells"; exit 2; }
n_worse=$(declared_diffs | grep -cxF "$DECLARED_REPLAY_WORSE")
[ "$n_worse" = 1 ] || { echo "REFUSED: the declared WORSE cell [$DECLARED_REPLAY_WORSE] is in the declared list $n_worse time(s), want 1"; exit 2; }
echo "=== G4 RESIDUAL GATE: $REPLAY_RESIDUAL_GATE (checked); $n_g4 DIFFERENCES DECLARED as a SET (walk $EXPECT_REPLAY_DIFFS_WALK / ssn $EXPECT_REPLAY_DIFFS_SSN / ipm $EXPECT_REPLAY_DIFFS_IPM), every cell a U0 cell, the one declared WORSE cell [$DECLARED_REPLAY_WORSE] among them"
# E2FR2 (d): NO DECLARED CLASS B OBJECT MAY HAVE A SOURCE MODIFIED IN THE RANGE,
# library or test, and each list's length is the declared class B count. An object
# path maps to its source as <dir before CMakeFiles>/<path after <target>.dir/> less
# ".o", normalized (so tests/CMakeFiles/x.dir/__/src/a.cpp.o -> src/a.cpp).
obj_source() {
  local o="$1" pre post
  pre=${o%%CMakeFiles/*}; post=${o#*CMakeFiles/*.dir/}; post=${post%.o}
  realpath -m --relative-to="$ROOT" "$ROOT/$pre$post"
}
for cfg in rel dbg; do
  if [ $cfg = rel ]; then names="$EXPECT_CLASS_B_NAMES_RELEASE"; want=${EXPECT_PSYM_CLASS_B%/*}
  else names="$EXPECT_CLASS_B_NAMES_DEBUG"; want=${EXPECT_PSYM_CLASS_B#*/}; fi
  n=$(printf '%s\n' $names | grep -c .)
  [ "$n" = "$want" ] || { echo "REFUSED: $cfg class B names list has $n objects, class B is DECLARED $want"; exit 2; }
  for o in $names; do
    case "$o" in
      */Unity/*)
        # A generated unity TU has no source of its own: its constituents are the
        # target's sources under the object's source directory, and none may be
        # modified in the range.
        src=${o%%CMakeFiles/*}; src=${src%/}
        [ -d "$src" ] || { echo "REFUSED: $cfg class B unity $o: no source directory $src"; exit 2; } ;;
      *)
        src=$(obj_source "$o")
        [ -f "$src" ] || { echo "REFUSED: $cfg class B $o maps to $src, which does not exist"; exit 2; } ;;
    esac
    if git diff --quiet "$BEFORE..$HEAD_AT_START" -- "$src"; then :; else
      echo "REFUSED: $cfg class B $o has its source $src MODIFIED in $BEFORE..$HEAD_AT_START (it is class A)"; exit 2
    fi
  done
done
# PREV (b), by value: each declared Debug NOISE-ONLY class A object's source IS modified.
for o in $EXPECT_CLASS_A_NOISE_ONLY_DEBUG; do
  src=$(obj_source "$o")
  [ -f "$src" ] || { echo "REFUSED: declared Debug NOISE-ONLY class A $o maps to $src, which does not exist"; exit 2; }
  printf '%s\n' $MODIFIED_CXX | grep -qxF "$src" \
    || { echo "REFUSED: declared Debug NOISE-ONLY class A $o: its source $src is not MODIFIED in the range"; exit 2; }
done
# By value: each declared class B library section mover is unmodified in the range and
# is named in its configuration's class B list.
for o in $EXPECT_LIB_CLASS_B_RELEASE $EXPECT_LIB_CLASS_B_DEBUG; do
  src=${o#CMakeFiles/hven.dir/}; src=${src%.o}
  if printf '%s\n' $lib_a | grep -qxF "$o"; then
    echo "REFUSED: $o is declared a class B library mover but its source $src is MODIFIED (class A)"; exit 2
  fi
done
for o in $EXPECT_LIB_CLASS_B_RELEASE; do printf '%s\n' $EXPECT_CLASS_B_NAMES_RELEASE | grep -qxF "$o" \
  || { echo "REFUSED: Release class B library mover $o is not in the Release class B list"; exit 2; }; done
for o in $EXPECT_LIB_CLASS_B_DEBUG; do printf '%s\n' $EXPECT_CLASS_B_NAMES_DEBUG | grep -qxF "$o" \
  || { echo "REFUSED: Debug class B library mover $o is not in the Debug class B list"; exit 2; }; done
echo "=== CLASS_B_NAMES_OK: Release $(printf '%s\n' $EXPECT_CLASS_B_NAMES_RELEASE | grep -c .) / Debug $(printf '%s\n' $EXPECT_CLASS_B_NAMES_DEBUG | grep -c .) declared objects, none with a source modified in the range"
# E2FR2 (f), by value: the baselines exist and are non-empty; no test source changed
# between the baselines' head and BEFORE; the ADDED list has no duplicate and each of
# its names is declared by a TEST line in a test source MODIFIED in the range.
for f in "$NAMES_BASE_DIR/suite-debug.txt" "$NAMES_BASE_DIR/suite-release.txt"; do
  nf=$(suite_names "$f" | grep -c .)
  [ "$nf" -gt 0 ] || { echo "REFUSED: by-name baseline $f names no started test"; exit 2; }
done
n_tsrc=$(git diff --name-only "$NAMES_BASE_HEAD..$BEFORE" -- 'tests/' | grep -c .)
[ "$n_tsrc" = 0 ] || { echo "REFUSED: $n_tsrc test file(s) changed between the by-name baseline $NAMES_BASE_HEAD and BEFORE $BEFORE"; exit 2; }
n_added=$(added_names | grep -c .); n_added_u=$(added_names | uniq | grep -c .)
[ "$n_added" -gt 0 ] && [ "$n_added" = "$n_added_u" ] || { echo "REFUSED: the ADDED list is empty or has duplicates ($n_added / $n_added_u)"; exit 2; }
while IFS= read -r t; do
  base=${t#*/}; base=${base%/*}; suite=${base%%.*}; name=${base#*.}
  n_decl=$(git diff "$BEFORE..$HEAD_AT_START" -- 'tests/*.cpp' | grep -E '^\+TEST(_P)?\(' | grep -cF "$suite, $name)")
  [ "$n_decl" = 1 ] || { echo "REFUSED: ADDED $t: its TEST line ($suite, $name) is added $n_decl time(s) in the range, want 1"; exit 2; }
done < <(added_names)
# By value (carried): the REMOVED list has no duplicate and no name also ADDED; each of its
# names is removed by exactly one TEST line in the range; and the range removes exactly as
# many TEST lines as the list names (a rename is one removed and one added TEST line).
# E4 (e): the REMOVED list is NOT empty this round -- 24 names -- and the carried checks are
# exactly the ones that make a non-empty list safe: each declared REMOVED name is removed by
# exactly one TEST line in the range, and the range removes exactly as many TEST lines as the
# list names, so a rename that the declaration missed cannot pass as an addition.
n_rem_names=$(removed_names | grep -c .); n_rem_u=$(removed_names | uniq | grep -c .)
[ "$n_rem_names" = "$n_rem_u" ] || { echo "REFUSED: the REMOVED list has duplicates ($n_rem_names / $n_rem_u)"; exit 2; }
n_both=$(LC_ALL=C comm -12 <(added_names) <(removed_names) | grep -c .)
[ "$n_both" = 0 ] || { echo "REFUSED: $n_both name(s) are both ADDED and REMOVED"; exit 2; }
while IFS= read -r t; do
  base=${t#*/}; base=${base%/*}; suite=${base%%.*}; name=${base#*.}
  n_decl=$(git diff "$BEFORE..$HEAD_AT_START" -- 'tests/*.cpp' | grep -E '^-TEST(_P)?\(' | grep -cF "$suite, $name)")
  [ "$n_decl" = 1 ] || { echo "REFUSED: REMOVED $t: its TEST line ($suite, $name) is removed $n_decl time(s) in the range, want 1"; exit 2; }
done < <(removed_names)
n_removed=$(git diff "$BEFORE..$HEAD_AT_START" -- 'tests/*.cpp' | grep -cE '^-TEST(_P)?\(')
[ "$n_removed" = "$n_rem_names" ] || { echo "REFUSED: $n_removed TEST line(s) removed in the range; this round declares $n_rem_names"; exit 2; }
for cfg in debug release; do
  bt=$(sed -nE 's/^.*tests passed, [0-9]+ tests failed out of ([0-9]+)$/\1/p' "$NAMES_BASE_DIR/suite-$cfg.txt" | grep -c .)
  [ "$bt" = 1 ] || { echo "REFUSED: the $cfg baseline carries $bt ctest summary lines, want 1"; exit 2; }
  btot=$(sed -nE 's/^.*tests passed, [0-9]+ tests failed out of ([0-9]+)$/\1/p' "$NAMES_BASE_DIR/suite-$cfg.txt")
  if [ $cfg = debug ]; then want=$EXPECT_DEBUG_TESTS; else want=$EXPECT_RELEASE_TESTS; fi
  [ "$want" = "$((btot + n_added - n_rem_names))" ] \
    || { echo "REFUSED: $cfg declared $want != the baseline's $btot + $n_added ADDED - $n_rem_names REMOVED"; exit 2; }
done
echo "=== BY-NAME: baselines $NAMES_BASE_DIR/suite-{debug,release}.txt ($(suite_names "$NAMES_BASE_DIR/suite-debug.txt" | grep -c .) / $(suite_names "$NAMES_BASE_DIR/suite-release.txt" | grep -c .) started names); $n_added ADDED, each added by exactly one TEST line in the range; $n_rem_names REMOVED, each removed by exactly one TEST line; $n_removed TEST lines removed"
# E4 (g), by value: the ADDED non-.cpp files under tests/ EQUAL the declared list, each is
# under the fixture directory, and that directory is named exactly once in the build files
# (the outside target's compile definition), so nothing in the P-SYM universe reads them.
got_files=$(git diff --name-only --diff-filter=A "$BEFORE..$HEAD_AT_START" -- tests/ | grep -v '\.cpp$' | LC_ALL=C sort)
want_files=$(printf '%s\n' "$ADDED_TEST_FILES" | grep -v '^$' | LC_ALL=C sort)
[ "$got_files" = "$want_files" ] \
  || { echo "REFUSED: the ADDED non-.cpp test files [$(printf '%s ' $got_files)] != declared [$(printf '%s ' $want_files)]"; exit 2; }
n_out_fx=$(printf '%s\n' "$want_files" | grep -vc '^tests/qp/fixtures/e2-d4/')
[ "$n_out_fx" = 0 ] || { echo "REFUSED: $n_out_fx declared ADDED test file(s) outside tests/qp/fixtures/e2-d4/"; exit 2; }
n_fx=$(git grep -hF 'fixtures/e2-d4' "$HEAD_AT_START" -- 'CMakeLists.txt' '*/CMakeLists.txt' '*.cmake' | grep -c .)
[ "$n_fx" = 1 ] || { echo "REFUSED: fixtures/e2-d4 is named $n_fx time(s) in the build files, want 1"; exit 2; }
n_fx_blk=$(awk -v t="$OUTSIDE_TARGET" '$0 ~ "target_compile_definitions\\("t"([[:space:]]|$)" {f=1} f {print} f && /\)/ {f=0}' tests/CMakeLists.txt | grep -cF 'fixtures/e2-d4')
[ "$n_fx_blk" = 1 ] || { echo "REFUSED: fixtures/e2-d4 is not named in $OUTSIDE_TARGET's compile definitions ($n_fx_blk)"; exit 2; }
# E4 (d), by value: tests/CMakeLists.txt adds EXACTLY the declared outside sources as
# non-comment lines and removes no non-comment line. THIS ROUND ADDS NO SOURCE AT ALL and
# tests/CMakeLists.txt does not change, so the derived want-list is empty and the check runs
# on an empty declaration -- the derivation NC1 ran, not a weakened check.
cm_add=$(git diff "$BEFORE..$HEAD_AT_START" -- tests/CMakeLists.txt | grep -E '^\+[^+]' | sed 's/^+//' | grep -vE '^[[:space:]]*(#|$)' | sed 's/^[[:space:]]*//; s/[[:space:]]*$//' | LC_ALL=C sort | tr '\n' ' ')
cm_del=$(git diff "$BEFORE..$HEAD_AT_START" -- tests/CMakeLists.txt | grep -E '^-[^-]' | sed 's/^-//' | grep -vE '^[[:space:]]*(#|$)' | grep -c .)
cm_want=$(for f in $EXPECT_ADDED_OUTSIDE_UNIVERSE; do printf '%s\n' "${f#tests/}"; done | LC_ALL=C sort | tr '\n' ' ')
[ "$cm_add" = "$cm_want" ] && [ "$cm_del" = 0 ] \
  || { echo "REFUSED: tests/CMakeLists.txt adds non-comment lines [$cm_add] (declared [$cm_want]) and removes $cm_del (declared 0)"; exit 2; }
echo "=== ADDED TEST FILES (checked): non-.cpp [$(printf '%s ' $got_files)] == declared, under the fixture directory named once in $OUTSIDE_TARGET's compile definitions; tests/CMakeLists.txt adds exactly [$cm_add] and removes no non-comment line"
echo "=== PREFLIGHT_VALUE_OK: class A $n_mod - $EXPECT_CLASS_A_OUTSIDE_UNIVERSE outside == declared $EXPECT_PSYM_CLASS_A; ADDED $n_add == declared $EXPECT_ADDED_CXX ($n_add_u inside the universe == declared $EXPECT_PSYM_ADDED); class B library objects all unmodified; G4 gate $REPLAY_RESIDUAL_GATE"

# shellcheck source=gate_asserts_m3b.sh
. "$ASSERTS" || exit 90

# A GATE SEGMENT IS JUDGED BY THE DIRECTORY IT RESOLVES, NOT BY ITS PROSE.
#
# Every segment below is a single-quoted heredoc whose SECOND line is its whole
# prologue: `ROOT=...; S=${GATES_SCRATCH:?}; G=$S/gates`. This evaluates THAT
# LINE in a subshell, with `GATES_SCRATCH` set exactly as the segment will see
# it, and asserts that the `$S` and `$G` the segment will really write to are
# this chain's own. It is a POSITIVE assertion, so a segment that names no path
# at all and still resolves somewhere else -- an unset export, a typo in it --
# fails too. Run 1's guard could only say "I found nothing bad".
#
# The residual textual check keeps the part that is genuinely about paths: a
# literal `.scratch/` in the segment's CODE, plus any re-assignment of S or G
# after the prologue. COMMENTS ARE STRIPPED BEFORE EITHER LOOKS. Run 2 of this
# chain was refused because a comment in `g5.sh` contained the WORD `scratch`;
# a sentence about a path is not a path, and a grep for a word cannot tell them
# apart.
assert_segment_scratch() {
  local f="$1" line probe got_s got_g code hits reassign rc=0
  if [ ! -s "$f" ]; then
    echo "SEGMENT REFUSED $f: was not written, or is empty"
    return 1
  fi
  line=$(sed -n '2p' "$f")
  case "$line" in
    *'S=${GATES_SCRATCH'*) : ;;
    *) echo "SEGMENT REFUSED $f: line 2 is not the GATES_SCRATCH prologue: $line"
       return 1 ;;
  esac
  # THE VALUES THE SEGMENT WILL ACTUALLY USE, obtained by running its own line.
  probe=$(GATES_SCRATCH="$GATES_SCRATCH" bash -c "$line"'; printf "%s\n%s\n" "$S" "$G"' 2>&1) || {
    echo "SEGMENT REFUSED $f: its prologue would not evaluate: $probe"
    return 1
  }
  got_s=$(printf '%s\n' "$probe" | sed -n '1p')
  got_g=$(printf '%s\n' "$probe" | sed -n '2p')
  if [ "$got_s" != "$GATES_SCRATCH" ]; then
    echo "SEGMENT REFUSED $f: resolves S='$got_s'; this chain's is '$GATES_SCRATCH'"
    rc=1
  fi
  if [ "$got_g" != "$GATES_SCRATCH/gates" ]; then
    echo "SEGMENT REFUSED $f: resolves G='$got_g'; this chain's is '$GATES_SCRATCH/gates'"
    rc=1
  fi
  code=$(sed -e 's/^[[:space:]]*#.*$//' "$f")
  hits=$(printf '%s\n' "$code" | grep -n '\.scratch/')
  if [ -n "$hits" ]; then
    echo "SEGMENT REFUSED $f: its CODE names a .scratch/ path of its own:"
    echo "$hits"
    rc=1
  fi
  reassign=$(printf '%s\n' "$code" | sed '1,2d' | grep -nE '(^|[;[:space:]])[SG]=')
  if [ -n "$reassign" ]; then
    echo "SEGMENT REFUSED $f: it re-assigns S or G after its prologue:"
    echo "$reassign"
    rc=1
  fi
  [ $rc -eq 0 ] && echo "SEGMENT_OK $f: resolves S=$got_s  G=$got_g"
  return $rc
}

run_locked() { # label, then the command
  local label="$1"; shift
  flock /tmp/box-build.lock bash -c '
    set -o pipefail
    label="$1"; shift
    echo "FOREGROUND_START ${label} $(date -Is)"
    foreign=$(pgrep -x hven_bench_cell | tr "\n" " ")
    [ -z "$foreign" ] && echo "PGREP_EMPTY ${label}: no foreign cell binary under my lock" \
                      || echo "NOTE: a foreign cell binary is alive under my lock: $foreign"
    "$@"
    rc=$?
    echo "FOREGROUND_END ${label} $(date -Is) rc=${rc}"
    exit ${rc}
  ' _ "$label" "$@"
  local rc=$?
  echo "WRAPPER_RC ${label} rc=${rc}"
  return $rc
}

# ===========================================================================
# ALL SIX SEGMENTS ARE WRITTEN FIRST, AND ALL SIX ARE CHECKED, BEFORE ANY LEG
# RUNS. Run 2 of this chain wrote and checked each segment immediately above
# its own leg, so it spent 26 minutes -- a Debug suite, a 673 s test, a Release
# suite and the replay -- before discovering that `g5.sh` would be refused. A
# pre-flight that can only be reached by paying for the legs in front of it is
# not a pre-flight. Nothing below writes to the build trees; `emit_segments`
# only writes the six `.sh` files.
# ===========================================================================
emit_segments() {
# --- G1: Debug -------------------------------------------------------------
cat > "$S/g1.sh" <<'INNER'
set -o pipefail
ROOT=/home/ghecht/Projects/hven; S=${GATES_SCRATCH:?}; G=$S/gates
rm -rf "$S/gates-dbg"
cmake -S "$ROOT" -B "$S/gates-dbg" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DHVEN_BUILD_TESTS=ON > "$G/configure-debug.log" 2>&1 || exit 91
cmake --build "$S/gates-dbg" -j "$(nproc)" > "$G/build-debug.log" 2>&1 || exit 91
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 ctest --test-dir "$S/gates-dbg" -j4 --no-tests=error \
  -E "Continuation.ProbeBudgetBoundsAFailingProposal" --output-on-failure > "$G/suite-debug.log" 2>&1
rc=$?; tail -6 "$G/suite-debug.log"; exit $rc
INNER

# --- G2: the long test, ALONE ---------------------------------------------
cat > "$S/g2.sh" <<'INNER'
set -o pipefail
ROOT=/home/ghecht/Projects/hven; S=${GATES_SCRATCH:?}; G=$S/gates
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 ctest --test-dir "$S/gates-dbg" --no-tests=error \
  -R "^Continuation.ProbeBudgetBoundsAFailingProposal$" --output-on-failure > "$G/suite-debug-long.log" 2>&1
rc=$?; tail -8 "$G/suite-debug-long.log"; exit $rc
INNER

# --- G3: Release ----------------------------------------------------------
cat > "$S/g3.sh" <<'INNER'
set -o pipefail
ROOT=/home/ghecht/Projects/hven; S=${GATES_SCRATCH:?}; G=$S/gates
rm -rf "$S/gates-rel"
cmake -S "$ROOT" -B "$S/gates-rel" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DHVEN_BUILD_TESTS=ON -DHVEN_BUILD_BENCH=ON \
  > "$G/configure-release.log" 2>&1 || exit 91
cmake --build "$S/gates-rel" -j "$(nproc)" > "$G/build-release.log" 2>&1 || exit 91
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 ctest --test-dir "$S/gates-rel" -j4 --no-tests=error \
  --output-on-failure > "$G/suite-release.log" 2>&1
rc=$?; tail -6 "$G/suite-release.log"
exit $rc
INNER

# --- G4: the replay -------------------------------------------------------
cat > "$S/g4.sh" <<'INNER'
set -o pipefail
ROOT=/home/ghecht/Projects/hven; S=${GATES_SCRATCH:?}; G=$S/gates
GATE="$1"
BIN=$S/gates-rel/bench/hven_sqp_corpus
D=$G/replay; mkdir -p "$D"; : > "$D/comparison.txt"
CELLS="$(grep -v '^#' "$ROOT/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt" | tr -d '\n')"
worst=0
for eng in walk ssn ipm; do
  MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
    "$BIN" --engine "$eng" --cells "$CELLS" --csv "$D/head-$eng.csv" > "$D/run-$eng.log" 2>&1
  rc=$?; echo "ran head-$eng rc=$rc"; [ $rc -eq 0 ] || { worst=1; break; }
done
[ $worst -eq 0 ] || exit 1
BASE=$ROOT/docs/notes/data/2026-09-m6-close-gate/replay
for eng in walk ssn ipm; do
  python3 -B "$ROOT/scripts/compare_replay.py" --residual-gate "$GATE" "$BASE/head-$eng.csv" "$D/head-$eng.csv" \
    "M6 CLOSE HEAD-$eng vs M7 W2 E4 HEAD-$eng (GATED $GATE)" | tee -a "$D/comparison.txt"
  rc=${PIPESTATUS[0]}; echo "  CMP_RC=$rc"
  # THIS ROUND DECLARES DIFFERENCES (E4 (h)), so the comparator's rc 1 is the EXPECTED
  # outcome on every engine and the GATE is the runner's set comparison on this
  # transcript. rc 0 -- no difference at all -- is as undeclared a shape as rc 2
  # (schema mismatch) and is refused here rather than read as success.
  [ $rc -eq 1 ] || { echo "  CMP_RC_UNDECLARED $eng=$rc (this round declares differences; 1 is the declared rc)"; worst=1; }
done
exit $worst
INNER

# --- G5: P-SYM, both configs ---------------------------------------------
cat > "$S/g5.sh" <<'INNER'
set -uo pipefail
ROOT=/home/ghecht/Projects/hven; S=${GATES_SCRATCH:?}; G=$S/gates
BD=$S/psym-build; OUT=$G/psym
# The section digest lives beside the checker in the scratch copy, for the same
# reason the classifier does: the `before` arm below checks out $BEFORE, at
# which this round's wrappers do not exist.
export DIGEST="$S/section_digest_fix2.py"
BEFORE="$1"; CHANGED_SOURCES="$2"; CHANGED_HEADERS="$3"; MIN_CONTROL="$4"
WANT_DIFFERING_RD="$5"; WANT_A_RD="$6"; WANT_B_RD="$7"
WANT_DEBUG_ONLY_RELEASE="$8"; WANT_DEBUG_ONLY_DEBUG="$9"
WANT_MOVERS_RELEASE="${10}"; WANT_MOVERS_DEBUG="${11}"
ADDED_CXX="${12}"; WANT_LIB_ADDED="${13}"; OUTSIDE_DIR="${14}"; OUTSIDE_SRCS="${15}"
WANT_B_DEBUG_ONLY="${16}"
ART=$ROOT/docs/notes/data/2026-09-m7-w2-m3b-minor-trace
# CLEARED, NOT MERELY CREATED (fix round 2). `psym_compare.sh capture` truncates
# its manifest before it may refuse an incomplete object universe, so a snapshot
# directory that survives from an earlier attempt is a live route to a narrowed
# comparison. Nothing under $OUT outlives this segment.
rm -rf "$OUT"
mkdir -p "$OUT"
HEAD_SHA=$(git -C "$ROOT" rev-parse HEAD)
BRANCH=$(git -C "$ROOT" rev-parse --abbrev-ref HEAD)
[ -z "$(git -C "$ROOT" status --porcelain)" ] || { echo "REFUSED: tree not clean"; exit 2; }
# THE CLASSIFIER AND THE ASSERTS ARE COPIED OUT OF THE TREE FIRST: the `before`
# arm below checks out $BEFORE, at which this round's wrappers do not exist.
cp "$ROOT/docs/notes/data/2026-09-m7-w2-m4-anticycling-arm/wrappers/classify_psym_m4.py" \
   "$S/classify_psym.py" || exit 93
cp "$ART/wrappers/gate_asserts_m3b.sh" "$S/gate_asserts.sh" || exit 93
cp "$ART/wrappers/check_sections_fix2.sh" "$S/check_sections.sh" || exit 93
cp "$ART/wrappers/section_digest_fix2.py" "$S/section_digest_fix2.py" || exit 93
export DIGEST="$S/section_digest_fix2.py"
. "$S/gate_asserts.sh" || exit 93
rc=0
for CFG in Release Debug; do
  if [ "$CFG" = "Release" ]; then
    WANT_DIFFERING=${WANT_DIFFERING_RD%/*}; WANT_A=${WANT_A_RD%/*}; WANT_B=${WANT_B_RD%/*}
    WANT_MOVERS="$WANT_MOVERS_RELEASE"
  else
    WANT_DIFFERING=${WANT_DIFFERING_RD#*/}; WANT_A=${WANT_A_RD#*/}; WANT_B=${WANT_B_RD#*/}
    WANT_MOVERS="$WANT_MOVERS_DEBUG"
  fi
  echo "=== P-SYM $CFG  after=$HEAD_SHA  before=$BEFORE  declared differing=$WANT_DIFFERING A=$WANT_A B=$WANT_B"
  rm -rf "$BD"
  cmake -S "$ROOT" -B "$BD" -G Ninja -DCMAKE_BUILD_TYPE="$CFG" \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DHVEN_BUILD_TESTS=ON -DHVEN_BUILD_BENCH=ON \
    > "$OUT/configure-$CFG.log" 2>&1 || exit 91
  CCACHE_DISABLE=1 cmake --build "$BD" -j "$(nproc)" > "$OUT/build-after-$CFG.log" 2>&1 || exit 91
  rm -rf "$OUT/after-$CFG"
  "$ROOT/scripts/psym_compare.sh" capture "$OUT/after-$CFG" "$BD" > "$OUT/capture-after-$CFG.log" 2>&1
  cap_after_rc=$?
  # THE PRODUCTION LIBRARY'S OBJECTS, KEPT (fix round 1). P-SYM compares its own
  # captured digests; the fail-closed NON-DEBUG SECTION check needs the objects
  # themselves, and they only exist while this build tree does.
  rm -rf "$OUT/after-obj-$CFG"; mkdir -p "$OUT/after-obj-$CFG"
  (cd "$BD" && find CMakeFiles/hven.dir -name '*.o' | LC_ALL=C sort) > "$OUT/libobjs-$CFG.txt"
  while IFS= read -r o; do
      mkdir -p "$OUT/after-obj-$CFG/$(dirname "$o")"
      cp "$BD/$o" "$OUT/after-obj-$CFG/$o"
  done < "$OUT/libobjs-$CFG.txt"
  restore() { git -C "$ROOT" checkout -q "$BRANCH" 2>/dev/null || git -C "$ROOT" checkout -q "$HEAD_SHA"; }
  trap restore EXIT
  git -C "$ROOT" checkout -q --detach "$BEFORE" || exit 92
  # E1 CHANGE (1): re-configure at BEFORE, then remove every output the BEFORE
  # manifest no longer names, so no after-arm object survives into the capture.
  cmake -S "$ROOT" -B "$BD" > "$OUT/configure-before-$CFG.log" 2>&1 || { restore; exit 91; }
  ninja -C "$BD" -t cleandead > "$OUT/cleandead-$CFG.log" 2>&1 || { restore; exit 91; }
  echo "CLEANDEAD_$CFG: $(grep -c 'Remove' "$OUT/cleandead-$CFG.log") output(s) removed"
  CCACHE_DISABLE=1 cmake --build "$BD" -j "$(nproc)" > "$OUT/build-before-$CFG.log" 2>&1 || { restore; exit 91; }
  rm -rf "$OUT/before-$CFG"
  "$ROOT/scripts/psym_compare.sh" capture "$OUT/before-$CFG" "$BD" > "$OUT/capture-before-$CFG.log" 2>&1
  cap_before_rc=$?
  rm -rf "$OUT/before-obj-$CFG"; mkdir -p "$OUT/before-obj-$CFG"
  while IFS= read -r o; do
      mkdir -p "$OUT/before-obj-$CFG/$(dirname "$o")"
      [ -f "$BD/$o" ] && cp "$BD/$o" "$OUT/before-obj-$CFG/$o"
  done < "$OUT/libobjs-$CFG.txt"
  git -C "$ROOT" checkout -q "$BRANCH"
  trap - EXIT
  # BOTH CAPTURE RETURN CODES ENTER THE GATE (fix round 2): the first round
  # discarded them, so a capture that refused an incomplete object universe was
  # invisible and its short manifest went on to `compare`.
  assert_capture "after-$CFG" "$cap_after_rc" "$OUT/after-$CFG" || rc=1
  assert_capture "before-$CFG" "$cap_before_rc" "$OUT/before-$CFG" || rc=1
  # E1 CHANGE (2): after = before + ADDED, and P-SYM's ONLY-AFTER set must be
  # exactly the ADDED sources' objects (by basename).
  nb=$(wc -l < "$OUT/before-$CFG/.psym-manifest"); na=$(wc -l < "$OUT/after-$CFG/.psym-manifest")
  want_added=$(for f in $ADDED_CXX; do basename "$f"; done | sed 's|$|.o|' | LC_ALL=C sort | tr '\n' ' ')
  n_added=$(printf '%s' "$want_added" | wc -w)
  assert_eq "capture pair $CFG after = before + ADDED" "$na" "$((nb + n_added))" || rc=1
  # E2 CHANGE (4'): the MODIFIED sources outside the universe. The after build
  # log must show their objects COMPILED, and the after capture must name NONE
  # of that target's objects. The object path is the target directory plus the
  # source's path below its CMake source directory (the first path component).
  for f in $OUTSIDE_SRCS; do
    if grep -qF "$OUTSIDE_DIR/${f#*/}.o" "$OUT/build-after-$CFG.log"; then
      echo "ASSERT OK   outside-universe $CFG: $OUTSIDE_DIR/${f#*/}.o was BUILT in the after arm"
    else
      echo "ASSERT FAIL outside-universe $CFG: the after build did not compile $OUTSIDE_DIR/${f#*/}.o"; rc=1
    fi
  done
  n_in=$(grep -c "^$OUTSIDE_DIR/" "$OUT/after-$CFG/.psym-manifest")
  assert_eq "outside-universe $CFG: objects of $OUTSIDE_DIR in the after manifest" "$n_in" 0 || rc=1
  echo "tree restored to $(git -C "$ROOT" rev-parse --abbrev-ref HEAD) $(git -C "$ROOT" rev-parse --short HEAD)"
  "$ROOT/scripts/psym_compare.sh" compare "$OUT/before-$CFG" "$OUT/after-$CFG" > "$OUT/compare-$CFG.txt" 2>&1
  cmp_rc=$?
  echo "PSYM_COMPARE_RC_$CFG=$cmp_rc (1 is EXPECTED; the gate is the classification)"
  got_only_after=$(sed -n 's/^ONLY-AFTER  *\([^ ]*\).*/\1/p' "$OUT/compare-$CFG.txt" | xargs -r -n1 basename | LC_ALL=C sort | tr '\n' ' ')
  if [ "$got_only_after" = "$want_added" ]; then
    echo "ASSERT OK   ONLY-AFTER $CFG: [$got_only_after] == the ADDED sources' objects"
  else
    echo "ASSERT FAIL ONLY-AFTER $CFG: [$got_only_after] != ADDED [$want_added]"
    rc=1
  fi
  # shellcheck disable=SC2086
  python3 -B "$S/classify_psym.py" --compare "$OUT/compare-$CFG.txt" --build "$BD" \
    --repo "$ROOT" --base "$BEFORE" --head "$HEAD_SHA" \
    --changed-sources $CHANGED_SOURCES --changed-headers $CHANGED_HEADERS \
    --expect-differing "$WANT_DIFFERING" \
    --min-negative-control "$MIN_CONTROL" > "$OUT/classification-$CFG.txt" 2>&1
  cls_rc=$?
  echo "CLASSIFY_RC_$CFG=$cls_rc"
  grep -E "^(HEADER |  repo|  base|  head|  cmd|  list|P-SYM|DIFFERING|  class|  UNEXPLAINED|  NEGATIVE|REFUSED)" "$OUT/classification-$CFG.txt"
  assert_psym "$CFG" "$OUT/classification-$CFG.txt" "$cmp_rc" "$cls_rc" "$MIN_CONTROL" \
    "$WANT_DIFFERING" "$WANT_A" "$WANT_B" || rc=1

  # ---- G5b: THE FAIL-CLOSED NON-DEBUG SECTION CHECK (fix round 1) ----------
  # The review's Important 1. This is the arm that carries the "nothing but
  # debug information changed" claim, and it carries it by FAILING on any
  # non-debug byte rather than by passing on the bytes three other tools
  # happened to inspect. It runs over ALL library objects in BOTH
  # configurations, not only over the Debug objects that differ.
  # E1 CHANGE (3): an ADDED library object is asserted present after and absent
  # before, and the section check runs over the objects present on both sides.
  got_lib_added=""
  while IFS= read -r o; do
    [ -f "$OUT/before-obj-$CFG/$o" ] || got_lib_added="$got_lib_added$o "
  done < "$OUT/libobjs-$CFG.txt"
  if [ "$got_lib_added" = "$WANT_LIB_ADDED" ]; then
    echo "ASSERT OK   library ADDED $CFG: [$got_lib_added] present after, absent before"
  else
    echo "ASSERT FAIL library ADDED $CFG: [$got_lib_added] != derived [$WANT_LIB_ADDED]"
    rc=1
  fi
  grep -vxF -f <(printf '%s\n' $WANT_LIB_ADDED) "$OUT/libobjs-$CFG.txt" > "$OUT/libobjs-both-$CFG.txt"
  bash "$S/check_sections.sh" "$CFG" "$OUT/before-obj-$CFG" "$OUT/after-obj-$CFG" \
    "$OUT/libobjs-both-$CFG.txt" > "$OUT/sections-$CFG.txt" 2>&1
  sec_rc=$?
  echo "SECTIONS_RC_$CFG=$sec_rc"
  grep -E "^(SECTIONS_|NON-DEBUG-MOVER|DEBUG-ONLY|REFUSED)" "$OUT/sections-$CFG.txt" | head -40
  # G5b: the NON-DEBUG-MOVER set must EQUAL the declared set, with no
  # refusal. The checker is M5's, unchanged; it exits 1 whenever a mover exists.
  got_movers=$(sed -n 's/^NON-DEBUG-MOVER  *\([^ ]*\).*/\1/p' "$OUT/sections-$CFG.txt" | LC_ALL=C sort | tr '\n' ' ')
  got_refused=$(sed -n 's/^SECTIONS_'"$CFG"'_REFUSED=//p' "$OUT/sections-$CFG.txt")
  echo "NON_DEBUG_MOVERS_GOT_$CFG=[$got_movers]  DECLARED=[$WANT_MOVERS]  REFUSED=$got_refused"
  if [ -z "$WANT_MOVERS" ]; then want_sec_rc=0; else want_sec_rc=1; fi
  if [ "$sec_rc" -ne "$want_sec_rc" ] || [ "$got_refused" != "0" ] || [ "$got_movers" != "$WANT_MOVERS" ]; then
    echo "ASSERT FAIL sections $CFG: rc=$sec_rc (want $want_sec_rc), refused='$got_refused', movers [$got_movers] != declared [$WANT_MOVERS]"
    rc=1
  else
    echo "ASSERT OK   sections $CFG: the non-debug movers are EXACTLY the declared set, nothing refused"
  fi
  # AND THE DECLARED DEBUG-ONLY SHAPE, per configuration, from the runner's
  # header: Release declares ZERO objects may differ at all; Debug declares
  # exactly the ONE TU whose own .cpp source lines moved. An object beyond the
  # declared count is a shape that was not declared, and is re-derived and
  # declared again, never discovered.
  got_dbgonly=$(sed -n 's/^SECTIONS_'"$CFG"'_DEBUG_ONLY=//p' "$OUT/sections-$CFG.txt")
  want_dbgonly=$([ "$CFG" = "Debug" ] && echo "$WANT_DEBUG_ONLY_DEBUG" || echo "$WANT_DEBUG_ONLY_RELEASE")
  if [ "$got_dbgonly" != "$want_dbgonly" ]; then
    echo "ASSERT FAIL sections $CFG: DEBUG_ONLY is '$got_dbgonly' and this round DECLARED '$want_dbgonly'"
    rc=1
  else
    echo "ASSERT OK   sections $CFG: DEBUG_ONLY=$got_dbgonly, as declared"
  fi
done
# E2 CHANGE (5'): THE DECLARED DEBUG-ONLY CLASS B. Fail-closed on three counts
# before the shared assert sees anything: (a) Debug B minus Release B is EXACTLY
# the declared list and Release B minus Debug B is EMPTY; (b) every listed
# object is NOISE-ONLY in the Release classification (its Release bytes moved
# only by __LINE__-class noise); (c) the Debug copy handed to the shared assert
# differs from the original by EXACTLY the listed class B lines. With an empty
# list the shared assert runs over the original, as in every earlier round.
XDBG="$OUT/classification-Debug.txt"
if [ -n "$WANT_B_DEBUG_ONLY" ]; then
  psym_set "$OUT/classification-Release.txt" B > "$OUT/classB-Release.txt"
  psym_set "$OUT/classification-Debug.txt" B > "$OUT/classB-Debug.txt"
  got_dbg_only=$(LC_ALL=C comm -13 "$OUT/classB-Release.txt" "$OUT/classB-Debug.txt" | tr '\n' ' ')
  got_rel_only=$(LC_ALL=C comm -23 "$OUT/classB-Release.txt" "$OUT/classB-Debug.txt" | tr '\n' ' ')
  want_dbg_only=$(printf '%s\n' $WANT_B_DEBUG_ONLY | LC_ALL=C sort | tr '\n' ' ')
  if [ "$got_dbg_only" = "$want_dbg_only" ] && [ -z "$got_rel_only" ]; then
    echo "ASSERT OK   class B asymmetry: Debug-only [$got_dbg_only] == declared; Release-only []"
  else
    echo "ASSERT FAIL class B asymmetry: Debug-only [$got_dbg_only] (declared [$want_dbg_only]), Release-only [$got_rel_only]"
    rc=1
  fi
  noise_rel=$(sed -n '/^  NOISE-ONLY -- /,/^$/p' "$OUT/classification-Release.txt")
  # E4 CHANGE (5''): the declared object must be NOT A MOVER in Release, and WHICH of the two
  # non-mover classes it is in is named per object. The E2 rounds admitted only NOISE-ONLY;
  # this round's object is BYTE-IDENTICAL in Release, which is a narrower fact than the check
  # accepted, so the admission is widened -- see the EXPECT_PSYM_B_DEBUG_ONLY comment for what
  # that does and does not weaken. The reading off the negative control is only sound if that
  # listing is COMPLETE, because the classifier prints at most 80 of it; asserted, not assumed.
  ctrl_rel=$(sed -n '/^  NEGATIVE CONTROL/,$p' "$OUT/classification-Release.txt")
  ctrl_n=$(printf '%s\n' "$ctrl_rel" | sed -n 's/^  NEGATIVE CONTROL.*: \([0-9]*\)$/\1/p')
  ctrl_p=$(printf '%s\n' "$ctrl_rel" | grep -cE '^      [^ ]')
  if [ -n "$ctrl_n" ] && [ "$ctrl_n" = "$ctrl_p" ]; then
    echo "ASSERT OK   class B asymmetry: the Release negative-control listing is COMPLETE ($ctrl_p of $ctrl_n printed)"
  else
    echo "ASSERT FAIL class B asymmetry: the Release negative-control listing is '$ctrl_p of $ctrl_n' -- a byte-identical verdict cannot be read off a truncated listing"
    rc=1
  fi
  for o in $WANT_B_DEBUG_ONLY; do
    if printf '%s\n' "$noise_rel" | grep -qE "^      (LIB|test) +$o\$"; then
      echo "ASSERT OK   class B asymmetry: $o is NOISE-ONLY in Release (its Release bytes moved, by __LINE__-class noise only)"
    elif printf '%s\n' "$ctrl_rel" | grep -qxF "      $o"; then
      echo "ASSERT OK   class B asymmetry: $o is BYTE-IDENTICAL in Release (named in the Release NEGATIVE CONTROL)"
    else
      echo "ASSERT FAIL class B asymmetry: $o is a MOVER in Release -- it is in neither the NOISE-ONLY list nor the negative control"; rc=1
    fi
  done
  XDBG="$OUT/classification-Debug.xcfg.txt"
  awk -v drop="$WANT_B_DEBUG_ONLY" '
    BEGIN { n = split(drop, d, " "); for (i = 1; i <= n; i++) keep[d[i]] = 1 }
    /^  class B -- / { sec = "B"; print; next }
    /^  [^ ]/        { sec = "" }
    { s = $0; sub(/^ +/, "", s)
      if (sec == "B" && /^      [^ ]/ && (s in keep)) next
      print }
  ' "$OUT/classification-Debug.txt" > "$XDBG"
  removed=$(diff "$OUT/classification-Debug.txt" "$XDBG" | sed -n 's/^< *//p' | LC_ALL=C sort | tr '\n' ' ')
  if [ "$removed" = "$want_dbg_only" ]; then
    echo "ASSERT OK   class B asymmetry: the cross-config copy removes exactly [$removed]"
  else
    echo "ASSERT FAIL class B asymmetry: the cross-config copy removed [$removed], declared [$want_dbg_only]"; rc=1
  fi
fi
# shellcheck disable=SC2086
assert_psym_cross_config "$OUT/classification-Release.txt" "$XDBG" \
  -- $CHANGED_SOURCES || rc=1
echo "G5_ASSERT_RC=$rc"
exit $rc
INNER

# --- G6: smoke, export contract, format, whitespace ------------------------
cat > "$S/g6.sh" <<'INNER'
set -o pipefail
ROOT=/home/ghecht/Projects/hven; S=${GATES_SCRATCH:?}; G=$S/gates
CHANGED="$1"; ASSERTS="$2"
cd "$ROOT" || exit 90
# THE ACCUMULATION IS THE RUNNER'S OWN FUNCTION (fix round 2), so the fold that
# proves "a failed sub-gate survives the green ones after it" runs THIS code.
# shellcheck source=gate_asserts_m3b.sh
. "$ASSERTS" || exit 90
rc=0
# THE SMOKE'S WORKDIR IS KEPT so the export contract can read the SAME build's
# installed hvenTargets.cmake and the SAME build's configure log -- the script's
# own documented contract for the third argument (never a literal picked ahead
# of time).
rm -rf "$S/smoke-work"
HVEN_INSTALL_SMOKE_WORKDIR="$S/smoke-work" bash scripts/check_install_smoke.sh > "$G/install-smoke.log" 2>&1
sub=$?; echo "INSTALL_SMOKE_RC=$sub"; tail -4 "$G/install-smoke.log"; rc=$(accumulate_rc "$rc" "$sub")
ALIGN=$(grep -o "EIGEN_MAX_ALIGN_BYTES resolved to [0-9]*" "$S/smoke-work/hven-configure.log" | tail -1 | awk '{print $NF}')
echo "ALIGN_FROM_THIS_BUILDS_CONFIGURE_LOG=$ALIGN"
[ -n "$ALIGN" ] || { echo "REFUSED: no EIGEN_MAX_ALIGN_BYTES in this build's configure log"; rc=1; }
TARGETS="$S/smoke-work/hven-install/lib64/cmake/hven/hvenTargets.cmake"
ls -l "$TARGETS"; sub=$?; rc=$(accumulate_rc "$rc" "$sub")
bash scripts/check_export_contract.sh "$TARGETS" linux "$ALIGN" > "$G/export-contract.log" 2>&1
sub=$?; echo "EXPORT_CONTRACT_RC=$sub"; tail -3 "$G/export-contract.log"; rc=$(accumulate_rc "$rc" "$sub")
echo "--- clang-format on this round's changed C++ files, derived from git"
# shellcheck disable=SC2086
clang-format --dry-run -Werror $CHANGED
sub=$?; echo "CLANG_FORMAT_RC=$sub"; rc=$(accumulate_rc "$rc" "$sub")
git diff --check
sub=$?; echo "DIFF_CHECK_RC=$sub"; rc=$(accumulate_rc "$rc" "$sub")
dirty=$(git status --porcelain | wc -l)
echo "DIRTY_FILES=$dirty"
[ "$dirty" -eq 0 ] || rc=1
echo "G6_ASSERT_RC=$rc"
exit "$rc"
INNER
}

# ===========================================================================
# THE PRE-FLIGHT. Write all six, judge all six, and stop here if any one of
# them would write somewhere other than this chain's own tree.
# ===========================================================================
emit_segments || { echo "REFUSED: a gate segment could not be written"; exit 2; }
preflight_rc=0
for seg in g1 g2 g3 g4 g5 g6; do
  assert_segment_scratch "$S/$seg.sh" || preflight_rc=1
done
if [ "$preflight_rc" -ne 0 ]; then
  echo "REFUSED: the pre-flight rejected a gate segment. NO LEG HAS RUN."
  exit 2
fi
echo "=== PREFLIGHT_OK: 6 segments written and asserted against GATES_SCRATCH, before the first leg ==="
if [ "$PREFLIGHT_ONLY" -eq 1 ]; then
  echo "PREFLIGHT_ONLY: stopping before G1. No leg was run and no build tree was touched."
  exit 0
fi

# --- G1: Debug -------------------------------------------------------------
run_locked G1-debug bash "$S/g1.sh"; rc1=$?
assert_ctest "G1 Debug suite" "$G/suite-debug.log" "$EXPECT_DEBUG_TESTS" || rc1=1
assert_names "G1 Debug suite" "$G/suite-debug.log" "$NAMES_BASE_DIR/suite-debug.txt" || rc1=1

# --- G2: the long test, ALONE ---------------------------------------------
run_locked G2-debug-long bash "$S/g2.sh"; rc2=$?
assert_ctest "G2 long test" "$G/suite-debug-long.log" "$EXPECT_LONG_TESTS" || rc2=1

# --- G3: Release ----------------------------------------------------------
run_locked G3-release bash "$S/g3.sh"; rc3=$?
assert_ctest "G3 Release suite" "$G/suite-release.log" "$EXPECT_RELEASE_TESTS" || rc3=1
assert_names "G3 Release suite" "$G/suite-release.log" "$NAMES_BASE_DIR/suite-release.txt" || rc3=1

# --- G4: the replay -------------------------------------------------------
run_locked G4-replay bash "$S/g4.sh" "$REPLAY_RESIDUAL_GATE"; rc4=$?
assert_replay_declared "$G/replay/comparison.txt" "$EXPECT_REPLAY_CELLS" "$EXPECT_REPLAY_COLUMNS" || rc4=1

# --- G5: P-SYM, both configs ---------------------------------------------
run_locked G5-psym bash "$S/g5.sh" "$BEFORE" "$CHANGED_SOURCES" "$CHANGED_HEADERS" \
  "$MIN_NEGATIVE_CONTROL" "$EXPECT_PSYM_DIFFERING" "$EXPECT_PSYM_CLASS_A" \
  "$EXPECT_PSYM_CLASS_B" "$EXPECT_DEBUG_ONLY_RELEASE" "$EXPECT_DEBUG_ONLY_DEBUG" \
  "$EXPECT_LIB_MOVERS_RELEASE" "$EXPECT_LIB_MOVERS_DEBUG" "$ADDED_IN_UNIVERSE" "$EXPECT_LIB_ADDED" \
  "$OUTSIDE_DIR" "$EXPECT_CLASS_A_OUTSIDE_SRCS $EXPECT_ADDED_OUTSIDE_UNIVERSE" "$EXPECT_PSYM_B_DEBUG_ONLY"; rc5=$?

# E2FR2 (d): the MEASURED class B set EQUALS the declared names, per configuration.
for CFG in Release Debug; do
  if [ $CFG = Release ]; then names="$EXPECT_CLASS_B_NAMES_RELEASE"; else names="$EXPECT_CLASS_B_NAMES_DEBUG"; fi
  got=$(psym_set "$G/psym/classification-$CFG.txt" B | awk '{print $NF}' | grep -v '^$' | LC_ALL=C sort | tr '\n' ' ')
  want=$(printf '%s\n' $names | grep -v '^$' | LC_ALL=C sort | tr '\n' ' ')
  # PREV (b): the declared list may be EMPTY; the classification must then exist and name none.
  if [ -s "$G/psym/classification-$CFG.txt" ] && [ "$got" = "$want" ]; then
    echo "ASSERT OK   class B by name $CFG: the measured set equals the declared list [$want]"
  else
    echo "ASSERT FAIL class B by name $CFG: measured [$got] != declared [$want]"; rc5=1
  fi
done
# PREV (b): THE MEASURED CLASS A, BY SOURCE, per configuration: Release's sources are EXACTLY the
# in-universe MODIFIED .cpp (derived from git); Debug's are those less the declared NOISE-ONLY
# objects' sources, and each declared one is NOISE-ONLY in the Debug compare.
want_a_src=$(printf '%s\n' $MODIFIED_CXX | grep -vxF -f <(printf '%s\n' $EXPECT_CLASS_A_OUTSIDE_SRCS) | LC_ALL=C sort | tr '\n' ' ')
noise_a_src=$(for o in $EXPECT_CLASS_A_NOISE_ONLY_DEBUG; do obj_source "$o"; done | LC_ALL=C sort | tr '\n' ' ')
for CFG in Release Debug; do
  got=$(psym_set "$G/psym/classification-$CFG.txt" A | awk '{print $NF}' | while IFS= read -r o; do obj_source "$o"; done | LC_ALL=C sort | tr '\n' ' ')
  if [ $CFG = Release ]; then want="$want_a_src"
  else want=$(printf '%s\n' $want_a_src | grep -vxF -f <(printf '%s\n' $noise_a_src) | LC_ALL=C sort | tr '\n' ' '); fi
  if [ -n "$got" ] && [ "$got" = "$want" ]; then
    echo "ASSERT OK   class A by source $CFG: [$got]"
  else
    echo "ASSERT FAIL class A by source $CFG: measured [$got] != derived [$want]"; rc5=1
  fi
done
for o in $EXPECT_CLASS_A_NOISE_ONLY_DEBUG; do
  if grep -qxE "NOISE-ONLY +$o" "$G/psym/compare-Debug.txt"; then
    echo "ASSERT OK   Debug NOISE-ONLY class A: $o"
  else
    echo "ASSERT FAIL Debug NOISE-ONLY class A: $o is not NOISE-ONLY in the Debug compare"; rc5=1
  fi
done
# PREV (b): each declared unmodified library section mover is NOISE-ONLY in its configuration.
for CFG in Release Debug; do
  if [ $CFG = Release ]; then movers="$EXPECT_LIB_NOISE_MOVERS_RELEASE"; else movers="$EXPECT_LIB_NOISE_MOVERS_DEBUG"; fi
  for o in $movers; do
    if grep -qxE "NOISE-ONLY +$o" "$G/psym/compare-$CFG.txt"; then
      echo "ASSERT OK   library NOISE-ONLY mover $CFG: $o"
    else
      echo "ASSERT FAIL library NOISE-ONLY mover $CFG: $o is not NOISE-ONLY in the $CFG compare"; rc5=1
    fi
  done
done

# --- G6: smoke, export contract, format, whitespace ------------------------
# E2FR3 (f), carried: tests/*.h too -- ONE changes in this range,
# tests/sqp/support/border_test_utils.h. E4 (c) adds bench/*.cpp to the same derivation.
CHANGED_CXX=$(git diff --name-only "$BEFORE..$HEAD_AT_START" -- 'include/*.h' 'src/*.cpp' 'tests/*.cpp' 'tests/*.h' 'bench/*.cpp' | tr '\n' ' ')
echo "=== CHANGED C++ FOR CLANG-FORMAT: $CHANGED_CXX"
run_locked G6-smoke bash "$S/g6.sh" "$CHANGED_CXX" "$ASSERTS"; rc6=$?

echo "GATES_SUMMARY G1=$rc1 G2=$rc2 G3=$rc3 G4=$rc4 G5=$rc5 G6=$rc6"
echo "GATES_END $(date -Is)"
# rc5 IS IN THE AGGREGATE. The first round's final line omitted it, so a P-SYM
# segment that failed outright could not fail the chain. Since fix round 2 the
# aggregation itself is `gate_asserts_m3b.sh`'s, so the fold that proves it runs
# this exact code rather than a copy of it -- and a return code that is not an
# integer (an unset variable, which used to read as 0) is its own refusal.
gates_aggregate "$rc1" "$rc2" "$rc3" "$rc4" "$rc5" "$rc6"
exit $?
