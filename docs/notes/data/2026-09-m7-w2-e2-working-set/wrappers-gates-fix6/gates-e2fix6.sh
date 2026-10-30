#!/usr/bin/env bash
# M7 W2 E2 FIX ROUND 6 -- THE GATES on the fix-6 commit (R-E: a CONVERGED grey-band candidate
# test is re-read with the refinement run to its end on the same factor, and certified
# dependent when rho' + its rounding floor <= kDepTol; two new QpCounters; R-D retired; the
# ACOPR57 restoration QP closes; declared re-pins).
# This file is the previous round's gate runner (the W2 restoration-request round's
# wrappers-gates directory under docs/notes/data/), COPIED and re-pointed; that file is NOT
# edited. What changed, and nothing else:
#   FX6 (a) the scratch tree, the log name and the derivation file name;
#   FX6 (b) every declared value, DERIVED from this round's range (BEFORE = the previous
#     round's gate record commit, see psym-derivation-e2fix6.txt) and measured by a FRESH
#     P-SYM rehearsal on the FINAL pair; nothing is carried from an earlier round's counts;
#   FX6 (c) the outside-universe source is MODIFIED, not added:
#     tests/qp/test_qp_anti_cycling_arm.cpp, whose only object is hven_qp_walk_seam_tests's
#     (checked by value, below); no source is ADDED;
#   FX6 (d) the by-name baseline is DERIVED from git as before (the newest gate record's
#     suite transcripts committed at or before BEFORE, and the head they ran at = the parent
#     of the commit that added them), with the record glob widened to a `gates/` directory
#     as well as `gates-*/` (the previous round's record is `gates/`); the ADDED list is this
#     round's five instances and the REMOVED list is ONE (a rename);
#   FX6 (e) THE ADDED NON-C++ TEST FILES, DECLARED BY NAME: three E2-D4 dump fixtures under
#     tests/qp/fixtures/e2-d4/ (data read at run time through HVEN_QP_E2D4_FIXTURES, the
#     outside target's compile definition); the pre-flight refuses unless the derived set
#     EQUALS the list and the fixture directory is named exactly once in the build files;
#     tests/CMakeLists.txt does not change (the carried check with an empty list);
#   FX6 (f) the G4 comparison label (g4 segment, one string);
#   FX6 (g) the P-SYM shape this range measures (class A, the class B includers of the
#     changed headers by name, the configuration asymmetries and the library section
#     movers), each declared by name and checked by value, in the slots the previous runner
#     already carries.
# Carried unchanged in behaviour: the by-name suite check (newline-safe), the class B list by
# name, clang-format over tests/*.h, the outside-universe check (4'), the class B asymmetry
# admission (5'), the G4 residual gate (6), the measured class A by source after G5.
#
#   G1  Debug: fresh configure, build, suite WITHOUT the >600 s test
#   G2  the >600 s test ALONE (Continuation.ProbeBudgetBoundsAFailingProposal)
#   G3  Release: fresh configure, build, suite
#   G4  the U0 27-cell three-arm replay against the M6 close pin, residual-gated
#   G5  P-SYM, Release and Debug, classified against $BEFORE with a DERIVED
#       changed-header list, plus the fail-closed non-debug section check
#   G6  install smoke, the export contract, clang-format, git diff --check
#
# ---- THIS ROUND'S DECLARED EXPECTATIONS -------------------------------------
# The derivation is `psym-derivation-e2fix6.txt` (this directory), committed WITH
# this file and BEFORE the chain was detached. BEFORE = the previous round's gate record
# commit on m7; the range is the fix-6 commit and this declaration commit (docs only).
# Class A from git; class B, the section shape and the test counts measured on the final
# pair.
#
# WALL IS INFORMATIONAL. Nothing this file prints is a timing claim.
# =========================================================================
set -o pipefail
ROOT=/home/ghecht/Projects/hven
S=$ROOT/.scratch/m7w2e2fix6gates
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
#   * and `chain_gates_e2fix6.sh` does not repeat this path either: it ASKS for it
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
# `psym-derivation-e2fix6.txt` §3: the by-name baseline's totals plus the five ADDED
# instances less the one REMOVED below.
BEFORE=${BEFORE:-e07f8173}
EXPECT_DEBUG_TESTS=${EXPECT_DEBUG_TESTS:-2847}
EXPECT_RELEASE_TESTS=${EXPECT_RELEASE_TESTS:-2848}
EXPECT_LONG_TESTS=1
EXPECT_REPLAY_CELLS=27
EXPECT_REPLAY_COLUMNS=75
EXPECT_REPLAY_DIFFS=0
EXPECT_REPLAY_ENGINES=3
# CARRIED CHANGE (6): the comparator's calibrated floating-measure gate. 1e-5 relative
# with the 1e-13 absolute floor is the suite's (tests/sqp/test_corpus_cells.cpp,
# `is_residual_column`). It relaxes the twelve float-measure columns ONLY; strings,
# statuses and the 56 integer fields stay byte-exact.
REPLAY_RESIDUAL_GATE=${REPLAY_RESIDUAL_GATE:-1e-5}
MIN_NEGATIVE_CONTROL=1

# PREV/FX6 (d): THE SUITES BY NAME. The set of test names ctest STARTS (disabled ones included)
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
# line, never word-split. Five ADDED instances and one REMOVED, derived by name from this
# round's measured suites against the baseline (psym-derivation-e2fix6.txt §3); the rename
# ObjectiveInflatedMultiplierHidesASmallContradiction -> ...ContradictionIsCertifiedOn-
# TheIndependentWorkingSet is one REMOVED and one ADDED TEST line.
ADDED_TEST_NAMES='QpE2WorkingSet.AConvergedGreyPinOnDallasmIsReReadDependent
QpE2WorkingSet.AGreyPinOnYorknetThatIsIndependentStaysIndependent
QpE2WorkingSet.AGreyReReadSolvesTheAcopr57RestorationQp
QpEngine.ObjectiveInflatedMultiplierContradictionIsCertifiedOnTheIndependentWorkingSet
WsIndependenceCalibration.AGreyReReadNeverFlipsACandidateAboveKDepTol'
REMOVED_TEST_NAMES='QpEngine.ObjectiveInflatedMultiplierHidesASmallContradiction'
# FX6 (e): THE ADDED NON-C++-SOURCE TEST FILES, by name, one per line: the E2-D4 dump
# fixtures (data, read at run time through HVEN_QP_E2D4_FIXTURES by the outside target only).
ADDED_TEST_FILES=${ADDED_TEST_FILES-'tests/qp/fixtures/e2-d4/ACOPR57_def-qp4.txt
tests/qp/fixtures/e2-d4/DALLASM_def-qp0.txt
tests/qp/fixtures/e2-d4/YORKNET_def-qp1.txt'}
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

# ---- THIS ROUND'S EXPECTED P-SYM MOVER SHAPE, PER CONFIGURATION ----------------
# `psym-derivation-e2fix6.txt` §2. Written RELEASE/DEBUG; g5.sh splits them.
EXPECT_PSYM_DIFFERING=${EXPECT_PSYM_DIFFERING:-40/39}
EXPECT_PSYM_CLASS_A=${EXPECT_PSYM_CLASS_A:-6/5}
EXPECT_PSYM_CLASS_B=${EXPECT_PSYM_CLASS_B:-34/34}
# THE CLASS B OBJECTS BY NAME, per configuration (measured, both configs; the same 34: every
# includer of the changed QpCounters layout that differs by more than __LINE__-class noise).
EXPECT_CLASS_B_NAMES_RELEASE=${EXPECT_CLASS_B_NAMES_RELEASE-"CMakeFiles/hven.dir/src/core/ledger.cpp.o CMakeFiles/hven.dir/src/drivers/sqp_solver.cpp.o CMakeFiles/hven.dir/src/globalization/sqp/soc_elastic_restoration.cpp.o CMakeFiles/hven.dir/src/qp/ipqp_engine.cpp.o bench/CMakeFiles/hven_sqp_bench.dir/bench_scale.cpp.o bench/CMakeFiles/hven_sqp_f7_cold.dir/bench_f7_cold.cpp.o bench/CMakeFiles/hven_sqp_ipqp_e1_arm.dir/ipqp_e1_arm.cpp.o bench/CMakeFiles/hven_sqp_ssn_safeguard_probe.dir/ssn_safeguard_probe.cpp.o tests/drivers/CMakeFiles/hven_drivers_tests.dir/test_callback.cpp.o tests/drivers/CMakeFiles/hven_drivers_tests.dir/test_options.cpp.o tests/drivers/CMakeFiles/hven_drivers_tests.dir/test_threads.cpp.o tests/drivers/CMakeFiles/hven_drivers_tests.dir/test_warm_protocol.cpp.o tests/interior/CMakeFiles/hven_interior_tests.dir/Unity/unity_1_cxx.cxx.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_activity_telemetry.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_dense_oracle.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_elastic_rung_vehicle.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_eqp_solve.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_hs_battery.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_hs_sweeps.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_ipqp_acceptance.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_ipqp_dispatch.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_ipqp_engine.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_ipqp_trace.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_ledger.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_qp_engine_border.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_qp_engine_indefinite.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_qp_engine_tr.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_qp_warm_start.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_scale_smoke.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_sqp_solver.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_ssn_engine.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_trace_writer.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_warm_start.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_warm_start_battery.cpp.o"}
EXPECT_CLASS_B_NAMES_DEBUG=${EXPECT_CLASS_B_NAMES_DEBUG-"CMakeFiles/hven.dir/src/core/ledger.cpp.o CMakeFiles/hven.dir/src/drivers/sqp_solver.cpp.o CMakeFiles/hven.dir/src/globalization/sqp/soc_elastic_restoration.cpp.o CMakeFiles/hven.dir/src/qp/ipqp_engine.cpp.o bench/CMakeFiles/hven_sqp_bench.dir/bench_scale.cpp.o bench/CMakeFiles/hven_sqp_f7_cold.dir/bench_f7_cold.cpp.o bench/CMakeFiles/hven_sqp_ipqp_e1_arm.dir/ipqp_e1_arm.cpp.o bench/CMakeFiles/hven_sqp_ssn_safeguard_probe.dir/ssn_safeguard_probe.cpp.o tests/drivers/CMakeFiles/hven_drivers_tests.dir/test_callback.cpp.o tests/drivers/CMakeFiles/hven_drivers_tests.dir/test_options.cpp.o tests/drivers/CMakeFiles/hven_drivers_tests.dir/test_threads.cpp.o tests/drivers/CMakeFiles/hven_drivers_tests.dir/test_warm_protocol.cpp.o tests/interior/CMakeFiles/hven_interior_tests.dir/Unity/unity_1_cxx.cxx.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_activity_telemetry.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_dense_oracle.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_elastic_rung_vehicle.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_eqp_solve.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_hs_battery.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_hs_sweeps.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_ipqp_acceptance.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_ipqp_dispatch.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_ipqp_engine.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_ipqp_trace.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_ledger.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_qp_engine_border.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_qp_engine_indefinite.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_qp_engine_tr.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_qp_warm_start.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_scale_smoke.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_sqp_solver.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_ssn_engine.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_trace_writer.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_warm_start.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_warm_start_battery.cpp.o"}
# FX6 (g): THE CONFIGURATION-ASYMMETRIC CLASS A, DECLARED BY NAME. A MODIFIED source whose
# DEBUG object differs only by __LINE__-class immediates is NOISE-ONLY there, not class A:
# test_continuation.cpp's re-pins are integer immediates (200 -> 18, 11 -> 9), which P-SYM's
# Debug reading classes as noise (measured). Checked by value before the legs (its source is
# MODIFIED) and after G5 (NOISE-ONLY in the Debug compare; the class A sets by source).
EXPECT_CLASS_A_NOISE_ONLY_DEBUG=${EXPECT_CLASS_A_NOISE_ONLY_DEBUG-"tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_continuation.cpp.o"}
# CARRIED CHANGE (5'): the configuration-asymmetric class B, DECLARED BY NAME:
# EMPTY -- both sets are empty (derivation §2).
EXPECT_PSYM_B_DEBUG_ONLY=${EXPECT_PSYM_B_DEBUG_ONLY-""}
# The C++ sources ADDED against BEFORE (derived from git; declared count): NONE (FX6 (c)).
EXPECT_ADDED_CXX=${EXPECT_ADDED_CXX:-0}
EXPECT_ADDED_OUTSIDE_UNIVERSE=${EXPECT_ADDED_OUTSIDE_UNIVERSE-""}
EXPECT_PSYM_ADDED=${EXPECT_PSYM_ADDED:-0}
# The outside target: hven_qp_walk_seam_tests (an HVEN_TESTING recompile target, not a
# PSYM_GROUPS entry; checked by value). FX6 (c): its fixtures source is MODIFIED this round.
OUTSIDE_TARGET=hven_qp_walk_seam_tests
OUTSIDE_DIR=tests/CMakeFiles/$OUTSIDE_TARGET.dir
EXPECT_CLASS_A_OUTSIDE_SRCS=${EXPECT_CLASS_A_OUTSIDE_SRCS-"tests/qp/test_qp_anti_cycling_arm.cpp"}
EXPECT_CLASS_A_OUTSIDE_UNIVERSE=$(printf '%s' "$EXPECT_CLASS_A_OUTSIDE_SRCS" | wc -w)

# ---- THE DECLARED SECTION SHAPE, PER CONFIGURATION --------------------------
# The NON-DEBUG movers are the MODIFIED src/*.cpp objects (DERIVED below) plus these
# declared UNMODIFIED library objects, per configuration: the NOISE-ONLY slot (checked
# unmodified before the legs and NOISE-ONLY in that configuration's compare after G5) is EMPTY
# this round; the class B slot (FX6 (g), below) carries the four library includers of the
# changed QpCounters layout.
EXPECT_LIB_NOISE_MOVERS_RELEASE=${EXPECT_LIB_NOISE_MOVERS_RELEASE-""}
# FX6 (g): THE CLASS B LIBRARY SECTION MOVERS, per configuration (the E2 rounds' slot, restored):
# unmodified library objects whose non-debug sections move because QpCounters grew; each is
# checked unmodified and named in that configuration's class B list before the legs.
EXPECT_LIB_CLASS_B_RELEASE=${EXPECT_LIB_CLASS_B_RELEASE-"CMakeFiles/hven.dir/src/core/ledger.cpp.o CMakeFiles/hven.dir/src/drivers/sqp_solver.cpp.o CMakeFiles/hven.dir/src/globalization/sqp/soc_elastic_restoration.cpp.o CMakeFiles/hven.dir/src/qp/ipqp_engine.cpp.o"}
EXPECT_LIB_CLASS_B_DEBUG=${EXPECT_LIB_CLASS_B_DEBUG-"CMakeFiles/hven.dir/src/core/ledger.cpp.o CMakeFiles/hven.dir/src/drivers/sqp_solver.cpp.o CMakeFiles/hven.dir/src/globalization/sqp/soc_elastic_restoration.cpp.o CMakeFiles/hven.dir/src/qp/ipqp_engine.cpp.o"}
EXPECT_LIB_NOISE_MOVERS_DEBUG=${EXPECT_LIB_NOISE_MOVERS_DEBUG-""}
EXPECT_DEBUG_ONLY_RELEASE=${EXPECT_DEBUG_ONLY_RELEASE:-0}
EXPECT_DEBUG_ONLY_DEBUG=${EXPECT_DEBUG_ONLY_DEBUG:-6}

mkdir -p "$G" "$S/logs"
LOG=$S/logs/gates-e2fix6.log
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

# THE ROUND'S CHANGED SURFACE, DERIVED FROM GIT rather than typed: the header
# list is what the classifier checks its own declared list against, and the
# source list is what class A is keyed on.
HEAD_AT_START=$(git rev-parse HEAD)
echo "=== HEAD_AT_START=$HEAD_AT_START"
CHANGED_HEADERS=$(git diff --name-only "$BEFORE..$HEAD_AT_START" -- include/ | tr '\n' ' ')
CHANGED_SOURCES=$(git diff --name-only "$BEFORE..$HEAD_AT_START" -- src/ tests/ | tr '\n' ' ')
echo "=== CHANGED HEADERS: $CHANGED_HEADERS"
echo "=== CHANGED SOURCES: $CHANGED_SOURCES"
[ -n "$CHANGED_HEADERS" ] || { echo "REFUSED: no changed header under include/ against $BEFORE"; exit 2; }
[ -n "$CHANGED_SOURCES" ] || { echo "REFUSED: no changed source against $BEFORE"; exit 2; }

# THE MODIFIED AND THE ADDED SOURCES, DERIVED SEPARATELY. A MODIFIED source's
# object is a class A candidate; an ADDED source's object has no before-arm
# counterpart and is asserted ONLY-AFTER in G5.
MODIFIED_CXX=$(git diff --name-only --diff-filter=M "$BEFORE..$HEAD_AT_START" -- 'src/*.cpp' 'tests/*.cpp' | tr '\n' ' ')
ADDED_CXX=$(git diff --name-only --diff-filter=A "$BEFORE..$HEAD_AT_START" -- 'src/*.cpp' 'tests/*.cpp' | tr '\n' ' ')
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
# PREV (c): the same checks over the ADDED outside sources, which must be ADDED (not modified).
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
# PREV (c): the ADDED sources inside the universe are the ADDED ones less the declared outside ones.
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
echo "=== G4 RESIDUAL GATE: $REPLAY_RESIDUAL_GATE (checked); differences declared $EXPECT_REPLAY_DIFFS"
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
# FX6 (g), by value: each declared class B library section mover is unmodified in the range and
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
# PREV (d): the REMOVED list may be EMPTY this round; then the range must remove no TEST line.
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
# FX6 (e), by value: the ADDED non-.cpp files under tests/ EQUAL the declared list, each is
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
# PREV (e), by value: tests/CMakeLists.txt adds EXACTLY the declared outside sources as
# non-comment lines, each inside the outside target's add_executable block, and removes no
# non-comment line (its comment lines may change).
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
    "M6 CLOSE HEAD-$eng vs M7 W2 E2 fix6 HEAD-$eng (GATED $GATE)" | tee -a "$D/comparison.txt"
  rc=${PIPESTATUS[0]}; echo "  CMP_RC=$rc"; [ $rc -eq 0 ] || worst=1
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
  for o in $WANT_B_DEBUG_ONLY; do
    if printf '%s\n' "$noise_rel" | grep -qE "^      (LIB|test) +$o\$"; then
      echo "ASSERT OK   class B asymmetry: $o is NOISE-ONLY in Release"
    else
      echo "ASSERT FAIL class B asymmetry: $o is NOT NOISE-ONLY in Release"; rc=1
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
assert_replay "$G/replay/comparison.txt" "$EXPECT_REPLAY_CELLS" "$EXPECT_REPLAY_COLUMNS" \
  "$EXPECT_REPLAY_DIFFS" "$EXPECT_REPLAY_ENGINES" || rc4=1

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
# E2FR3 (f), carried: tests/*.h too (none changes in this range; the derivation still covers them).
CHANGED_CXX=$(git diff --name-only "$BEFORE..$HEAD_AT_START" -- 'include/*.h' 'src/*.cpp' 'tests/*.cpp' 'tests/*.h' | tr '\n' ' ')
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
