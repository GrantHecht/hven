#!/usr/bin/env bash
# M7 W2 BPS + QC-8 GATES RUN, BEFORE 4a0da559 -> HEAD (the declaration commit on top of f58d01dc; docs only, so every derivation
# below sees f58d01dc's sources). COPIED from the QN + QC-1 runner (docs/notes/data/2026-10-m7-qn-qc1/wrappers-gates/gates-qn-qc1.sh,
# NOT edited) and re-pointed. 4a0da559 is the QN + QC-1 record's commit (the previous GREEN gate record). The range holds a docs-only
# ledger commit (7f97fb1f), BPS (a781c3f2: the bordered answer's certificate, QpCounters::border_uncertified_fallbacks; riders d761d91f)
# and QC-8 (c65ab30e, b79b012f, b3dca346, ffab67c0, f58d01dc: the implied blocker, QpCounters::dep_implied_passovers). EVERY declared
# value below is DERIVED from that range (git) or MEASURED on the final pair 4a0da559 / f58d01dc by this declaration's own rehearsal
# (rehearsal/); NONE is carried.
# WHAT CHANGED against gates-qn-qc1.sh, AND NOTHING ELSE (psym-derivation-bps-qc8.txt, header):
#   (a) scratch .scratch/m7bpsqc8gates, log gates-bps-qc8.log, rc variable GATES_BPS_QC8_RC, G4 label "M7 BPS QC8", BEFORE 4a0da559, the
#       by-name baseline pinned to QN + QC-1's gates/ record (BEFORE-bounded); the uncapped classifier is a byte-identical copy;
#   (b) the changed surface: 4 headers, 10 modified .cpp (8 inside the P-SYM universe, 2 outside: tests/qp/test_qp_anti_cycling_arm.cpp,
#       tests/qp/test_qp_convex_certificate.cpp [hven_qp_walk_seam_tests]), 2 ADDED .cpp both OUTSIDE the universe (tests/qp/
#       test_qp_border_certificate.cpp, tests/qp/test_qp_implied_blocker.cpp [hven_qp_walk_seam_tests]; tests/CMakeLists.txt adds exactly
#       those two lines), SIX added fixture files under tests/qp/fixtures/e2-d4/, no src/ or tests/sqp CMake change (the two CMake
#       declarations are EMPTY); nothing is added inside the universe (EXPECT_PSYM_ADDED 0, EXPECT_ADDED_CXX 2);
#   (c) THE ONE STRUCTURAL CHANGE TO THE NAME CHECKS: a test may be RE-DECLARED in the range: its name survives while its TEST line is
#       removed and re-added (QC-8 renamed the N3 fixture's test to a new name and re-premised the old name's body below it, so git
#       shows one -TEST and one +TEST line of the SAME name). REDECLARED_TEST_NAMES declares it; the runner checks each such name is
#       removed once and added once in the range, is not ADDED or REMOVED by name, and the removed-TEST-line count equals REMOVED +
#       REDECLARED. The by-name set check (baseline - REMOVED + ADDED == this run) is unchanged and is what proves the name survived;
#   (c2) A NEW SLOT, EXPECT_CLASS_A_NOISE_ONLY_RELEASE: a class A object whose source changed but whose RELEASE bytes moved only by
#       __LINE__-class noise (tests/sqp/test_sqp_restoration.cpp: its edits are Debug-only #ifndef NDEBUG premises), the mirror of the carried
#       EXPECT_CLASS_A_NOISE_ONLY_DEBUG. Release class A = in-universe modified .cpp less this list; Debug's less the Debug list. The
#       pre-flight, the class A by-source check and a NOISE-ONLY-in-Release assertion are generalised accordingly (all carried code kept);
#   (d) G4 exact arm now names QN + QC-1's gates/replay (gates/replay/exact-vs-qn-qc1-record.txt); suites, class A/B, sections, ADDED
#       names, the G4 declaration: re-derived or re-measured (the slots below); the sentinel loop also covers the G4 declaration.
# The comments inside the body below that state earlier rounds' values are those rounds' text, kept for provenance; the VALUES in the code
# lines are this run's. The QN + QC-1 header follows.
# M7 QN (STAGES 1-3 + QUALITY) + QC-1 GATES RUN, BEFORE 7c24a78b -> HEAD (the declaration commit on top of 2d1011e4; docs only, so every
# derivation below sees 2d1011e4's sources). COPIED from the P1E + C13 runner (docs/notes/data/2026-10-m7-w2-p1e-c13/wrappers-gates/
# gates-p1e-c13.sh, NOT edited) and re-pointed. 7c24a78b is the P1E + C13 record's commit (the previous GREEN gate record). The range holds
# the quasi-Newton mode (83a7fe13, 7f9cd829, 772b5a54, e5054d92 + riders aa4667f0; 2956050c + riders 295c5a5b: SqpOptions::hessian_mode,
# default kExact; four new library sources, four new test sources) and QC-1 (2bf05c5c + riders 2d1011e4: no infeasibility certificate
# from a near-feasible restoration entry). EVERY declared value below is DERIVED from that range (git) or MEASURED on the final pair
# 7c24a78b / 2d1011e4 by this declaration's own rehearsal (rehearsal/); NONE is carried.
# WHAT CHANGED against gates-p1e-c13.sh, AND NOTHING ELSE (psym-derivation-bps-qc8.txt, header):
#   (a) scratch .scratch/m7bpsqc8gates, log gates-bps-qc8.log, G4 label "M7 BPS QC8", BEFORE 7c24a78b, by-name baseline pinned to P1E + C13's
#       gates/ record (BEFORE-bounded); the uncapped classifier is this directory's byte-identical copy;
#   (b) the changed surface: 13 headers (4 added), 14 modified .cpp (13 inside the P-SYM universe, ONE outside: tests/sqp/test_sqp_ladder_seams.cpp),
#       EIGHT added .cpp all inside (4 library, 4 hven_sqp_tests), no added non-.cpp test file;
#   (c) NEW pre-flight check EXPECT_SRC_CMAKE_ADDS / EXPECT_SRC_CMAKE_DELS on src/CMakeLists.txt; EXPECT_SQP_CMAKE_ADDS declares the four test sources;
#   (d) suite_names reads the two value-parameterised QN suites' instances by stem + ordinal (their ctest names embed an address);
#   (e) G4 records, NOT asserts, an exact comparison against the previous GREEN record's replay (gates/replay/exact-vs-qn-qc1-record.txt);
#   (f) the sentinel loop covers every measured slot; suites, class A/B, sections, ADDED names re-derived or re-measured.
# The comments inside the body below that state earlier rounds' values are those rounds' text, kept for provenance; the VALUES in the
# code lines are this run's. The P1E + C13 header follows.
# M7 W2 ROUND QN + QC-1 GATES RUN, BEFORE d495e15a -> HEAD (the declaration commit on top of feca4400; docs only, so every
# derivation below sees feca4400's sources). COPIED from the W1 + CERT-1 runner (docs/notes/data/2026-10-m7-w2-w1-cert1/
# wrappers-gates/gates-w1-cert1.sh, NOT edited) and re-pointed. d495e15a is the W1 + CERT-1 record's commit (the previous GREEN
# gate record). The range holds the ledger (16f02c0e, docs only), P1E d3d4c922 (the C11 YORKNET / SAWPATH fixtures: four dump
# files), P1E 63e31aad (three SqpOptions levers, default OFF: elastic_witness_start, elastic_last_optimal_rung,
# restoration_resume_reduction; SqpCounters 61 -> 63, SqpOptions 33 -> 36), C13 71fed56c (QpEngine::PassOverScan +
# detail::RowSubset, behaviour-identical) and feca4400 (comment riders). EVERY declared value below is DERIVED from that range
# (git) or MEASURED on the final pair d495e15a / feca4400 by this declaration's own rehearsal (rehearsal/); NONE is carried.
# WHAT CHANGED against gates-w1-cert1.sh, AND NOTHING ELSE:
#   (a) scratch .scratch/m7bpsqc8gates, log gates-bps-qc8.log, derivation psym-derivation-bps-qc8.txt, G4 label "M7 W2 BPS QC8",
#       BEFORE d495e15a, by-name baseline pinned to W1 + CERT-1's gates/ record (BEFORE-bounded); the uncapped classifier is
#       this directory's byte-identical copy of W1 + CERT-1's;
#   (b) the changed surface: FIVE headers, ELEVEN modified .cpp (ten inside the P-SYM universe, ONE outside:
#       tests/sqp/test_sqp_ladder_seams.cpp [hven_sqp_seam_tests]), ONE added .cpp INSIDE the universe
#       (tests/sqp/test_elastic_witness_start.cpp [hven_sqp_tests]; EXPECT_PSYM_ADDED 1, the ONLY-AFTER assertion and the
#       after = before + 1 manifest assertion are carried g5 code, exercised here with a non-zero count), NO added outside
#       source; tests/CMakeLists.txt is UNCHANGED and tests/sqp/CMakeLists.txt adds exactly the source line and the
#       HVEN_QP_E2D4_FIXTURES compile definition (a NEW pre-flight check, EXPECT_SQP_CMAKE_ADDS);
#   (c) the fixture pre-flight: FOUR added dump files (tests/qp/fixtures/e2-d4/C11_{YORKNET,SAWPATH}_qp{0,1}.txt); the fixture
#       directory is now named in TWO build-file lines (the walk seam target's and hven_sqp_tests's compile definitions): the
#       carried "named exactly once" check is changed to "exactly two, one in each declared reader target's definitions";
#   (d) suites 7 ADDED / 0 REMOVED, class A/B, sections, G4 list re-derived or re-measured (the slots below).
# The comments inside the body below that state W1 + CERT-1 / NC4 / NC3 values are those rounds' text, kept for provenance; the
# VALUES in the code lines are this run's. The W1 + CERT-1 header follows.
# M7 W2 ROUND W1 + CERT-1 GATES RUN, BEFORE 2d2f92b1 -> HEAD (the declaration commit on top of 6bcffcd9; docs only, so
# every derivation below sees 6bcffcd9's sources). COPIED from the NC4 + R2 runner
# (docs/notes/data/2026-10-m7-w2-nc4-r2/wrappers-gates/gates-nc4-r2.sh, NOT edited) and re-pointed.
# 2d2f92b1 is the NC4 + R2 record's commit (the previous GREEN gate record). The range holds the ledger (5f035075, docs
# only), CERT-1 (3c942780: the restoration's infeasibility certificate is stated in the convergence test's own
# feasibility measure) and W1: cee22473 (the BRITGAS qp1 fixture), 4ddeb90c (the ascent guard in the QP walk + the GPROJ
# rider in the SQP driver; QpCounters::ascent_rides, SqpCounters::nonfinite_box_projections), 1c4d59bd (guard v4),
# 6bcffcd9 (review riders: a fixture assertion + a comment). EVERY declared value below is DERIVED from that range (git)
# or MEASURED on the final pair 2d2f92b1 / 6bcffcd9 by this declaration's own rehearsal (rehearsal/); NONE is carried
# from NC4's.
# WHAT CHANGED against gates-nc4-r2.sh, AND NOTHING ELSE:
#   (a) scratch .scratch/m7bpsqc8gates, log gates-bps-qc8.log, derivation psym-derivation-bps-qc8.txt, G4 label
#       "M7 W2 BPS QC8", BEFORE 2d2f92b1, by-name baseline pinned to NC4 r2's gates/ record (BEFORE-bounded);
#   (b) THE CLASSIFIER CAP IS LIFTED. The shared classify_psym_m4.py prints at most 80 of its NEGATIVE CONTROL and NOISE-ONLY
#       test listings (`sorted(control)[:80]`); NC4 r2 relaxed the class B asymmetry check to cope. This runner instead
#       uses classify_psym_m4_uncapped.py (this directory; the shared file is byte-untouched, it differs from it in exactly
#       the two `[:80]` slices) and RESTORES the carried completeness assertion (printed == stated) in the asymmetry check,
#       so an ABSENT object is read as a mover again only off a COMPLETE listing, as before NC4. g5 and the chain take the
#       classifier from this directory;
#   (c) outside-universe sources: THREE MODIFIED (tests/qp/test_qp_saddle_exit.cpp, tests/qp/test_qp_unrun_dependency_test.cpp
#       [hven_qp_walk_seam_tests], tests/sqp/test_sqp_ladder_seams.cpp [hven_sqp_seam_tests]) and ONE ADDED
#       (tests/qp/test_qp_ascent_guard.cpp [hven_qp_walk_seam_tests]); the two outside targets are NC4's;
#   (d) suites, class A/B, sections, fixtures and the added-name list re-derived (see the slots below). ROOT is derived
#       from the file location (worktree run), as in NC4 r2.
# The comments inside the body below that state NC4 / NC3 values (counts, file names) are those rounds' text, kept for
# provenance; the VALUES in the code lines are this run's. The NC4 + R2 header follows.
# M7 W2 ROUND NC4 + R2 (+ R2 FIX 1), GATES RUN, BEFORE 79775010 -> HEAD (the declaration commit on top of
# 21369067; docs only, so every derivation below sees 21369067's sources). COPIED from the NC3 run-2 runner
# (docs/notes/data/2026-10-m7-w2-nc3-saddle-exit/wrappers-gates/gates-nc3-r2.sh, NOT edited) and re-pointed.
# 79775010 is the NC3 run-2 record's commit; its sources EQUAL NC3 fix 1's gated head b7993013.
# The range holds NC4 (37864475 fixtures + eb710598 the convex certificate), R2 (726a52a2 the failed
# subproblem's own step + the non-finite guard), 92d13c4e (ledger, docs only) and R2 fix 1 (98a5bec1 +
# 21369067). EVERY declared value below is DERIVED from that range (git) or MEASURED on the final pair
# 79775010 / 21369067 by this declaration's own rehearsal (rehearsal/); NONE is carried from NC3's.
# WHAT CHANGED against gates-nc3-r2.sh, AND NOTHING ELSE:
#   (a) scratch .scratch/m7bpsqc8gates, log gates-bps-qc8.log, derivation psym-derivation-bps-qc8.txt, G4 label
#       "M7 W2 BPS QC8", BEFORE 79775010, by-name baseline pinned to NC3 r2's gates-r2/ record;
#   (b) ROOT is DERIVED from this file's own location (git rev-parse --show-toplevel), not a literal: this
#       run executes in its own worktree, and the six emitted segments receive it by substitution of the
#       @ROOT@ placeholder on their prologue line (the segment-scratch assertion judges the RESOLVED values);
#   (c) TWO outside-universe targets, not one: the changed sources outside the P-SYM universe belong to
#       hven_qp_walk_seam_tests (modified test_qp_saddle_exit.cpp, ADDED test_qp_convex_certificate.cpp) AND to
#       hven_sqp_seam_tests (modified test_sqp_ladder_seams.cpp). OUTSIDE_TARGETS / OUTSIDE_DIRS are lists; every
#       outside source must be a source of exactly ONE listed target, named once in the build files, and g5
#       asserts it was compiled in exactly one listed target's directory and that NO listed target's object is
#       in the after manifest. Everything else in those checks is NC3's;
#   (d) suites, class A/B, sections, fixtures and the added-name list re-derived (see the slots below).
# The comments inside the body below that state NC3 values (counts, file names, "ten/eleven tests") are
# NC3 run 2's text, kept for provenance; the VALUES in the code lines are this run's.
# The NC3 run-2 header follows.
# M7 W2 ROUND NC3 -- RUN 2, THE SUCCESSOR DECLARATION (-r2) after FIX ROUND 1. BEFORE a522f115 -> HEAD
# (the declaration commit on top of b7993013; docs only, so every derivation below sees b7993013's sources).
# Three code commits: 1b644a62 (fixtures), 054e4c78 (the saddle exit), b7993013 (fix round 1: the
# anti-return rule covers every probe-exempt and point-memory-released member, plus ONE new test,
# QpNc3SaddleExit.ADegenerateSaddleVertexIsNeverCertifiedThroughTwoZeroLengthRides). Fix round 1 touches
# solver_counters.h, qp_engine.h, qp_engine.cpp and tests/qp/test_qp_saddle_exit.cpp, all already in run 1's
# range, so the changed SURFACE (files) is run 1's; its VALUES are not assumed -- every one is re-derived or
# re-measured on a522f115..b7993013. Run 1 (declaration a4390c4e on a522f115..054e4c78) ran GREEN and its
# files are byte-frozen; this is the E4 run-2 precedent (docs/notes/data/2026-09-m7-w2-e4-proximal-eqp/README.md):
# parallel *-r2 files, nothing of run 1 edited.
# WHAT CHANGED against run 1's gates-nc3.sh, AND NOTHING ELSE:
#   r2 (a) scratch .scratch/m7w2nc3gates-r2, log gates-nc3-r2.log, derivation psym-derivation-nc3-r2.txt, rc
#     variable GATES_NC3_R2_RC (chain), G4 label "M7 W2 NC3 R2";
#   r2 (b) THE SUITES: 11 ADDED test instances by name (run 1's ten plus the fix-round test), 0 REMOVED:
#     Debug 2893 + 11 = 2904, Release 2894 + 11 = 2905. SKIPPED set still = the P1 baseline's by name;
#   r2 (c) every P-SYM slot and G4 re-measured by a FRESH rehearsal on a522f115/b7993013 (rehearsal-r2/);
#   r2 (d) the by-name baseline is still resolved BEFORE-bounded (`git log -1 ... a522f115`), so run 1's NC3
#     gates/ record -- committed later -- cannot be picked; EXPECT_NAMES_BASE_REL still names the P1 record and
#     the runner refuses otherwise (fail-closed).
# The run-1 header follows, kept for provenance; where it states a run-1 value it is run 1's, not this run's.
# M7 W2 ROUND NC3 (THE SADDLE EXIT) -- THE GATES. BEFORE a522f115 -> HEAD (the declaration commit on
# top of 054e4c78; docs only, so every derivation below sees 054e4c78's sources).
#   1b644a62  qp(m7): W2 NC3 -- fixtures first: the C10 saddle-exit dumps (five fixture .txt files) and
#             the four driver fixtures' QP-failure premise re-taken from the walk that keeps the
#             trusted-kWrong dead end.
#   054e4c78  qp(m7): W2 NC3 -- THE SADDLE EXIT: a trusted-kWrong dead end is searched for a VERIFIED
#             negative-curvature direction of its face and ridden off it, instead of ending
#             kNumericalError. THREE new QpCounters fields (saddle_exits, saddle_exit_misses,
#             saddle_direction_solves) and four new QpWalkMinorRecord fields (saddle_exit,
#             saddle_curvature, saddle_slope, saddle_slope_tol).
# a522f115 is the P1 gate record's ledger commit; its sources EQUAL that round's last gated head
# 1f46e34d (`git diff --stat 1f46e34d a522f115 -- include src tests bench cmake scripts` EMPTY,
# OBSERVED). THIS RANGE IS THE FIRST MEASUREMENT OF NC3 AND P1 BUILT TOGETHER.
#
# THIS FILE IS P1's RUNNER (docs/notes/data/2026-10-m7-w2-p1-schur-cost/wrappers-gates/gates-p1.sh),
# COPIED AND RE-POINTED; that file is NOT edited. Every structural rule documented in
# docs/notes/data/2026-09-m7-w2-e4-proximal-eqp/README.md, docs/notes/data/2026-09-m7-w2-e2-fix7-fix8/
# README.md and docs/notes/data/2026-10-m7-w2-e2-fix9-standstill/README.md is carried unchanged in
# behaviour: the single scratch literal, the segment prologue check, the by-name suites (newline-safe),
# the class B list by name with the no-modified-source refusal (tests included), the widened class B
# asymmetry admission (5''), the negative-control completeness assertion, the G4 SET gate under the
# 1e-5 relative / 1e-13 absolute residual gate, the empty-clang-format-set refusal, bench/ in every
# source derivation, the by-name baseline pinned to a DECLARED record, the SKIPPED set asserted by name,
# and P1's blank-line filter on an empty added-fixture list.
# WHAT CHANGED, AND NOTHING ELSE (every value DERIVED from a522f115..054e4c78, never carried):
#   NC3 (a) the scratch tree (.scratch/m7w2nc3gates-r2), the log name (gates-nc3-r2.log), the derivation file
#     name (psym-derivation-nc3-r2.txt) and the G4 label ("M7 W2 NC3");
#   NC3 (b) the changed surface: FOUR headers (solver_counters.h, qp_engine.h, qp_walk_trace.h,
#     ws_independence.h), FIVE MODIFIED .cpp (src/qp/qp_engine.cpp, src/qp/ws_independence.cpp,
#     tests/sqp/test_qp_engine_indefinite.cpp, tests/sqp/test_sqp_solver.cpp -- four INSIDE the P-SYM
#     universe -- and tests/qp/test_qp_unrun_dependency_test.cpp, a source of the OUTSIDE target
#     hven_qp_walk_seam_tests), ONE ADDED .cpp (tests/qp/test_qp_saddle_exit.cpp, also a source of the
#     outside target; tests/CMakeLists.txt adds exactly that line), FIVE ADDED fixture files under
#     tests/qp/fixtures/e2-d4/, and one tests/ header (tests/qp/support/qp_dump.h, included only by
#     seam-target tests). No bench/, cmake/ or scripts/ change;
#   NC3 (c) THE SUITES: 10 ADDED test instances by name (nine plain TEST lines in suite QpNc3SaddleExit
#     and one in SqpDriverQpFailure), 0 REMOVED. Baseline = the P1 gate record (Debug 2893, Release
#     2894), so Debug 2903 / Release 2904. None is Release-only: the DEBUG-skipped addition list is
#     EMPTY, so the SKIPPED set must equal the baseline's exactly (Debug 7, Release 1);
#   NC3 (d) G4: the SAME 43-entry declared difference set (walk 17 / ssn 13 / ipm 13) -- RE-MEASURED
#     from this declaration's own Release build of 054e4c78, not carried;
#   NC3 (e) the P-SYM shape, MEASURED by a FRESH rehearsal on the final pair a522f115/054e4c78, both
#     configurations (psym-derivation-nc3-r2.txt section 2). The changed headers include solver_counters.h,
#     so nearly every object in the capture universe moves; the class B set is DERIVED by the classifier
#     and cross-checked against `ninja -t deps`;
#   NC3 (f) the by-name baseline must resolve to EXACTLY the P1 gates/ record.
#
#   G1  Debug: fresh configure, build, suite WITHOUT the >600 s test
#   G2  the >600 s test ALONE (Continuation.ProbeBudgetBoundsAFailingProposal)
#   G3  Release: fresh configure, build, suite
#   G4  the U0 27-cell three-arm replay against the M6 close pin, residual-gated, against
#       the DECLARED 43-difference SET
#   G5  P-SYM, Release and Debug, classified against $BEFORE with a DERIVED
#       changed-header list, plus the fail-closed non-debug section check
#   G6  install smoke, the export contract, clang-format, git diff --check
#
# The derivation is `psym-derivation-nc3-r2.txt` (this directory), committed WITH this file and
# BEFORE the chain was detached. RULE: nothing in it or here is adjusted after the chain's first
# leg.
#
# WALL IS INFORMATIONAL. Nothing this file prints is a timing claim.
# =========================================================================

set -o pipefail
ROOT=$(git -C "$(dirname "${BASH_SOURCE[0]}")" rev-parse --show-toplevel) || { echo "REFUSED: no worktree root"; exit 3; }
S=$ROOT/.scratch/m7bpsqc8gates
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
#   * and `chain_gates_nc3_r2.sh` does not repeat this path either: it ASKS for it
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
CLASSIFIER=$ROOT/docs/notes/data/2026-10-m7-bps-qc8/wrappers-gates/classify_psym_m4_uncapped.py
ASSERTS=$ART/wrappers/gate_asserts_m3b.sh


# ---- THIS ROUND'S EXPECTED COUNTS -------------------------------------------
# `psym-derivation-nc3-r2.txt` section 3: the by-name baseline's totals (the P1 gate record, Debug
# 2893 / Release 2894), less the 0 declared REMOVED instances, plus the 10 declared ADDED ones.
BEFORE=${BEFORE:-4a0da559}
EXPECT_DEBUG_TESTS=${EXPECT_DEBUG_TESTS:-3045}
EXPECT_RELEASE_TESTS=${EXPECT_RELEASE_TESTS:-3046}
EXPECT_LONG_TESTS=1
EXPECT_REPLAY_CELLS=27
EXPECT_REPLAY_COLUMNS=75
EXPECT_REPLAY_ENGINES=3
# NC3 (d): THE DECLARED DIFFERENCE SET, per engine and by coordinate -- E4's 43 (settler ruling
# E4-25 (b)), RE-MEASURED (run 2) on b7993013 from this declaration's own Release build
# (rehearsal-r2/rehearsal-r2-nc3-g4-comparison.txt) and found to be the same set; not carried. The gate is NOT a count on
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
# F8 (f), THE ONE STRUCTURAL CHANGE TO THE RUNNER BODY: E4's record commit ff02789b ADDED TWO
# suite records at once -- gates-run1/ (run 1, whose suites ran at 13013b4c) and gates/ (run 2,
# whose suites ran at bba2fba6 = ff02789b^) -- so the carried derivation returns TWO paths, and
# `dirname` of a two-line string is not a directory (the pre-flight would refuse on an empty
# baseline, fail-closed, but refuse a correct range). The newest commit's records are therefore
# narrowed to the CURRENT-RUN record `gates/` (the record round's own convention: a superseded run
# is filed as gates-run<N>/) whenever there is more than one, and the runner REFUSES unless exactly
# ONE path remains. Only gates/'s "head = parent of the record commit" reading is true here; the
# two records' started-name sets are identical (OBSERVED, psym-derivation-e2f8.txt section 3).
NAMES_BASE_ALL=$(git -C "$ROOT" log -1 --format= --name-only --diff-filter=A "$BEFORE" \
  -- 'docs/notes/data/*/gates-*/suite-release.txt' 'docs/notes/data/*/gates/suite-release.txt' \
  | grep -v '^$')
if [ "$(printf '%s\n' "$NAMES_BASE_ALL" | grep -c .)" -gt 1 ]; then
  NAMES_BASE_REL=$(printf '%s\n' "$NAMES_BASE_ALL" | grep -E '/gates/suite-release\.txt$')
else
  NAMES_BASE_REL=$NAMES_BASE_ALL
fi
[ "$(printf '%s\n' "$NAMES_BASE_REL" | grep -c .)" = 1 ] \
  || { echo "REFUSED: the by-name baseline is not ONE record: [$(printf '%s ' $NAMES_BASE_ALL)]"; exit 2; }
# NC3 (f): the ONE record must be the DECLARED one -- the P1 gates/ record, whose suites ran at
# 1f46e34d (sources == a522f115's == BEFORE's). A derivation that resolves anywhere else, ambiguous
# or not, is refused here rather than silently re-baselined.
EXPECT_NAMES_BASE_REL=docs/notes/data/2026-10-m7-qn-qc1/gates/suite-release.txt
[ "$NAMES_BASE_REL" = "$EXPECT_NAMES_BASE_REL" ] \
  || { echo "REFUSED: the by-name baseline resolved to [$NAMES_BASE_REL], declared [$EXPECT_NAMES_BASE_REL]"; exit 2; }
NAMES_BASE_DIR=$ROOT/$(dirname "$NAMES_BASE_REL")
NAMES_BASE_HEAD=$(git -C "$ROOT" rev-parse --short "$(git -C "$ROOT" log -1 --format=%H \
  --diff-filter=A "$BEFORE" -- "$NAMES_BASE_REL")^")
# ONE NAME PER LINE: a parameterised instance's ctest name carries spaces
# ("..../4-byte object <00-00 00-00>"), so the lists are newline-delimited and read line by
# line, never word-split. NC3 r2 (c): 11 ADDED instances and 0 REMOVED, DERIVED by name from this
# range's own TEST lines (eleven plain TEST lines, no TEST_P/TEST_F, so one ctest instance each) and checked
# against the baseline (psym-derivation-nc3-r2.txt section 3). NOTHING here is carried.
ADDED_TEST_NAMES='QpBpsBorderCertificate.Acopp57Entry1BorderUntouched
QpBpsBorderCertificate.Acopr118Entry9Border
QpBpsBorderCertificate.Acopr118Entry9Refactorize
QpBpsBorderCertificate.Acopr118Qp26Border
QpBpsBorderCertificate.Acopr118Qp26Refactorize
QpE2WorkingSet.TheDoubledRowIsImpliedByItsFaceAndTheWalkReachesTheOptimum
QpQc8ImpliedBlocker.DallasmEntry6Border
QpQc8ImpliedBlocker.DallasmEntry6Refactorize
QpQc8ImpliedBlocker.DallasmQp1Border
QpQc8ImpliedBlocker.DallasmQp1Refactorize
QpQc8ImpliedBlocker.DallasmQp7Border
QpQc8ImpliedBlocker.DallasmQp7Refactorize'
REMOVED_TEST_NAMES=''
# BPS + QC-8 (c): the names RE-DECLARED in the range: removed as a TEST line and added again as a TEST line of the SAME name (git shows
# the old name's line moved below the new test). Net by name: unchanged, so they are in neither ADDED nor REMOVED.
REDECLARED_TEST_NAMES=${REDECLARED_TEST_NAMES-'QpE2WorkingSet.NoAdmissibleMemberAndNoPassOverEndsInsideTheBox'}
redeclared_names() { printf '%s\n' "$REDECLARED_TEST_NAMES" | grep -v '^$' | LC_ALL=C sort; }
# NC3 (b): FIVE non-C++ test files are ADDED in this range, all fixtures under tests/qp/fixtures/e2-d4/
# (derived from git; the runner refuses unless the list EQUALS `git diff --diff-filter=A` over tests/
# less the .cpp files). tests/qp/fixtures/e2-d4/FORMAT.txt is MODIFIED, not added.
ADDED_TEST_FILES=${ADDED_TEST_FILES-'tests/qp/fixtures/e2-d4/BPS_ACOPP57_entry1.txt
tests/qp/fixtures/e2-d4/BPS_ACOPR118_entry9.txt
tests/qp/fixtures/e2-d4/BPS_ACOPR118_qp26.txt
tests/qp/fixtures/e2-d4/QC8_DALLASM_entry6.txt
tests/qp/fixtures/e2-d4/QC8_DALLASM_qp1.txt
tests/qp/fixtures/e2-d4/QC8_DALLASM_qp7.txt'}
# QN (c), THE ONE STRUCTURAL CHANGE TO THE NAME READER: the two value-parameterised QN suites (Battery/SqpQnHs, Battery/SqpQnVariantHs; 6 and
# 24 instances) are registered by ctest under names that embed the raw bytes of their parameter, and that parameter (kHsCases' HsCase)
# holds a const char* -- so the instance name carries a link-layout ADDRESS (observed: `16-byte object <03-00 00-00 00-00 00-00 FD-F0 4C-07 ..>`),
# which differs between the Debug and the Release binary and between builds. Every other suite's instance names are address-free (the older
# `4-byte object <00-00 00-00>` ones are ints). Such a name cannot be declared by value, so for exactly those two suites an instance is
# named by its STEM and its ORDINAL among the suite's instances in ctest test-number order (registration order, deterministic):
# `Battery/SqpQnHs.ConvergesWithoutAHessianCall/ordinal-3`. The ordinal reading is applied to the baseline, the chain's logs and the
# declaration's own derivation alike; the baseline holds no such name, so it changes nothing there.
suite_names() { sed -nE 's/^ +Start +([0-9]+): (.*[^ ]) *$/\1\t\2/p' "$1" | LC_ALL=C sort -n -k1,1 \
  | awk -F'\t' '{ n = $2; if (match(n, /^Battery\/SqpQn(Variant)?Hs\.[A-Za-z0-9_]+\//)) { stem = substr(n, 1, RLENGTH - 1); c[stem]++; n = stem "/ordinal-" c[stem] } print n }' \
  | grep -v '^$' | LC_ALL=C sort; }
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
# F9 (g), carried: THE SKIPPED SET, BY NAME, per configuration. ctest lists each test that did not
# run as "<n> - <name> (Skipped)" (GTEST_SKIP) or "(Disabled)"; a skip is inside ctest's denominator
# and is not a failure, so the totals and the started-name sets cannot see a test that silently
# stops running. Debug's skipped set must EQUAL the baseline's Debug skips PLUS the declared
# Release-only names below; Release's must EQUAL the baseline's Release skips.
# NC3: this range adds NO Release-only (Debug-skipped) test -- the eleven added tests carry no
# GTEST_SKIP (OBSERVED, `git diff a522f115 b7993013 -- tests bench | grep -c GTEST_SKIP` -> 0) -- so the
# list is EMPTY and both sets must equal the baseline's exactly.
DEBUG_ONLY_SKIPS_ADDED=''
skipped_names() { sed -nE 's/^[[:space:]]+[0-9]+ - (.*[^ ]) \(Skipped\)$/\1/p' "$@" | grep -v '^$' | LC_ALL=C sort -u; }
skip_added_names() { printf '%s\n' "$DEBUG_ONLY_SKIPS_ADDED" | grep -v '^$' | LC_ALL=C sort; }
assert_skips() {
  local label="$1" cfg="$2" base="$3" ng nb na d; shift 3
  [ -s "$base" ] || { echo "ASSERT FAIL $label skips: the baseline $base is missing"; return 1; }
  nb=$(skipped_names "$base" | grep -c .)
  [ "$nb" -gt 0 ] || { echo "ASSERT FAIL $label skips: the baseline names no Skipped test (the listing was not read)"; return 1; }
  if [ "$cfg" = debug ]; then na=$(skip_added_names | grep -c .); else na=0; fi
  ng=$(skipped_names "$@" | grep -c .)
  if [ "$cfg" = debug ]; then
    d=$(LC_ALL=C comm -3 <( (skipped_names "$base"; skip_added_names) | LC_ALL=C sort -u) <(skipped_names "$@"))
  else
    d=$(LC_ALL=C comm -3 <(skipped_names "$base") <(skipped_names "$@"))
  fi
  if [ "$ng" = "$((nb + na))" ] && [ -z "$d" ]; then
    echo "ASSERT OK   $label skips by name: $ng Skipped == the baseline's $nb + the $na declared Release-only"
  else
    echo "ASSERT FAIL $label skips by name: $ng Skipped vs the baseline's $nb + $na declared; the difference (expected '<' tab-indented '>' this run):"
    printf '%s\n' "$d" | head -20
    return 1
  fi
}
if [ "${1:-}" = --check-skips ]; then
  case "${2:-}" in debug|release) : ;; *) echo "usage: --check-skips debug|release LOG..."; exit 2 ;; esac
  cfg=$2; shift 2
  assert_skips "offline ${cfg}" "$cfg" "$NAMES_BASE_DIR/suite-${cfg}.txt" "${@:?LOG}" && exit 0
  exit 2
fi

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
# `psym-derivation-nc3-r2.txt` section 2. Written RELEASE/DEBUG; g5.sh splits them.
# NC3 (e): EVERY MEASURED SLOT BELOW IS RE-MEASURED on a522f115/b7993013 (run 2) by this declaration's own
# rehearsal (rehearsal-r2/rehearse-nc3-r2.sh then rehearsal-r2/offline-nc3-r2.sh); no value is a previous round's.
# A slot not yet measured carries the sentinel UNMEASURED, and the runner refuses on it below.
EXPECT_PSYM_DIFFERING=${EXPECT_PSYM_DIFFERING:-45/46}
EXPECT_PSYM_CLASS_A=${EXPECT_PSYM_CLASS_A:-7/8}
EXPECT_PSYM_CLASS_B=${EXPECT_PSYM_CLASS_B:-38/38}
# THE CLASS B OBJECTS BY NAME, per configuration -- the includers of the four changed headers (directly or
# transitively) whose source is NOT modified in the range; derived by the classifier from the build's
# own dependency data and cross-checked independently (psym-derivation-nc3-r2.txt section 2). The
# pre-flight refuses any object here whose source -- tests included -- is in the range.
EXPECT_CLASS_B_NAMES_RELEASE=${EXPECT_CLASS_B_NAMES_RELEASE-"CMakeFiles/hven.dir/src/core/ledger.cpp.o CMakeFiles/hven.dir/src/drivers/sqp_solver.cpp.o CMakeFiles/hven.dir/src/globalization/sqp/mixed_lift.cpp.o CMakeFiles/hven.dir/src/globalization/sqp/qn_lift_bridge.cpp.o CMakeFiles/hven.dir/src/globalization/sqp/soc_elastic_restoration.cpp.o CMakeFiles/hven.dir/src/qp/ipqp_engine.cpp.o bench/CMakeFiles/hven_sqp_f7_cold.dir/bench_f7_cold.cpp.o bench/CMakeFiles/hven_sqp_ipqp_e1_arm.dir/ipqp_e1_arm.cpp.o bench/CMakeFiles/hven_sqp_ssn_safeguard_probe.dir/ssn_safeguard_probe.cpp.o tests/drivers/CMakeFiles/hven_drivers_tests.dir/test_callback.cpp.o tests/drivers/CMakeFiles/hven_drivers_tests.dir/test_options.cpp.o tests/drivers/CMakeFiles/hven_drivers_tests.dir/test_threads.cpp.o tests/drivers/CMakeFiles/hven_drivers_tests.dir/test_warm_protocol.cpp.o tests/interior/CMakeFiles/hven_interior_tests.dir/Unity/unity_1_cxx.cxx.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_activity_telemetry.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_border_ops.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_dense_oracle.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_elastic_rung_vehicle.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_elastic_witness_start.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_eqp_refine_ab.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_eqp_solve.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_ipqp_acceptance.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_ipqp_dispatch.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_ipqp_engine.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_ipqp_trace.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_ledger.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_qp_engine.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_qp_engine_border.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_qp_engine_indefinite.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_qp_engine_tr.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_qp_lift_oracle.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_qp_warm_start.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_qp_ws_independence.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_scale_smoke.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_sqp_qn_mode.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_ssn_engine.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_trace_writer.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_warm_start.cpp.o"}
EXPECT_CLASS_B_NAMES_DEBUG=${EXPECT_CLASS_B_NAMES_DEBUG-"CMakeFiles/hven.dir/src/core/ledger.cpp.o CMakeFiles/hven.dir/src/drivers/sqp_solver.cpp.o CMakeFiles/hven.dir/src/globalization/sqp/mixed_lift.cpp.o CMakeFiles/hven.dir/src/globalization/sqp/qn_lift_bridge.cpp.o CMakeFiles/hven.dir/src/globalization/sqp/soc_elastic_restoration.cpp.o CMakeFiles/hven.dir/src/qp/ipqp_engine.cpp.o bench/CMakeFiles/hven_sqp_f7_cold.dir/bench_f7_cold.cpp.o bench/CMakeFiles/hven_sqp_ipqp_e1_arm.dir/ipqp_e1_arm.cpp.o bench/CMakeFiles/hven_sqp_ssn_safeguard_probe.dir/ssn_safeguard_probe.cpp.o tests/drivers/CMakeFiles/hven_drivers_tests.dir/test_callback.cpp.o tests/drivers/CMakeFiles/hven_drivers_tests.dir/test_options.cpp.o tests/drivers/CMakeFiles/hven_drivers_tests.dir/test_threads.cpp.o tests/drivers/CMakeFiles/hven_drivers_tests.dir/test_warm_protocol.cpp.o tests/interior/CMakeFiles/hven_interior_tests.dir/Unity/unity_1_cxx.cxx.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_activity_telemetry.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_border_ops.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_dense_oracle.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_elastic_rung_vehicle.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_elastic_witness_start.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_eqp_refine_ab.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_eqp_solve.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_ipqp_acceptance.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_ipqp_dispatch.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_ipqp_engine.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_ipqp_trace.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_ledger.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_qp_engine.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_qp_engine_border.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_qp_engine_indefinite.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_qp_engine_tr.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_qp_lift_oracle.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_qp_warm_start.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_qp_ws_independence.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_scale_smoke.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_sqp_qn_mode.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_ssn_engine.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_trace_writer.cpp.o tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_warm_start.cpp.o"}
# THE CONFIGURATION-ASYMMETRIC CLASS A, DECLARED BY NAME (a TU whose bytes are __LINE__-class
# noise-only in Debug but not Release): see the measured values above; the slot is EMPTY unless the
# derivation says otherwise.
EXPECT_CLASS_A_NOISE_ONLY_DEBUG=${EXPECT_CLASS_A_NOISE_ONLY_DEBUG-""}
# BPS + QC-8 (c2): the mirror slot, DECLARED BY NAME: a class A object that is NOISE-ONLY in Release (measured by this declaration's rehearsal).
EXPECT_CLASS_A_NOISE_ONLY_RELEASE=${EXPECT_CLASS_A_NOISE_ONLY_RELEASE-"tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_sqp_restoration.cpp.o"}
# CARRIED CHANGE (5'') (the widened class B asymmetry admission, E4 run 1): the
# configuration-asymmetric class B, DECLARED BY NAME. The declared object must be NOT A MOVER in
# Release (NOISE-ONLY or in the NEGATIVE CONTROL), named per object; the Release negative-control
# listing must be COMPLETE; the set difference must be EXACTLY the declared list. The slot is EMPTY
# unless the measured Debug and Release class B sets differ.
EXPECT_PSYM_B_DEBUG_ONLY=${EXPECT_PSYM_B_DEBUG_ONLY-""}
# NC3 (b): ONE .cpp is ADDED against BEFORE (derived from git; declared count 1) and it lies OUTSIDE
# the P-SYM universe (tests/qp/test_qp_saddle_exit.cpp, a source of hven_qp_walk_seam_tests), so
# NOTHING is added inside it. ONE of the five MODIFIED .cpp lies outside it too
# (tests/qp/test_qp_unrun_dependency_test.cpp, the same target); the other four -- two library
# (src/qp/qp_engine.cpp, src/qp/ws_independence.cpp) and two hven_sqp_tests sources -- are inside.
EXPECT_ADDED_CXX=${EXPECT_ADDED_CXX:-2}
EXPECT_ADDED_OUTSIDE_UNIVERSE=${EXPECT_ADDED_OUTSIDE_UNIVERSE-"tests/qp/test_qp_border_certificate.cpp tests/qp/test_qp_implied_blocker.cpp"}
EXPECT_PSYM_ADDED=${EXPECT_PSYM_ADDED:-0}
# P1E (b): the non-comment lines tests/sqp/CMakeLists.txt ADDS in this range (derived from git; none removed).
EXPECT_SQP_CMAKE_ADDS=${EXPECT_SQP_CMAKE_ADDS-''}
EXPECT_SRC_CMAKE_ADDS=${EXPECT_SRC_CMAKE_ADDS-''}
EXPECT_SRC_CMAKE_DELS=${EXPECT_SRC_CMAKE_DELS-''}
# The outside target: hven_qp_walk_seam_tests (an HVEN_TESTING recompile target, not a
# PSYM_GROUPS entry; checked by value). Both lists name one of its sources this round.
OUTSIDE_TARGETS="hven_qp_walk_seam_tests hven_sqp_seam_tests"
FIXTURE_TARGET=hven_qp_walk_seam_tests
OUTSIDE_DIRS=$(for t in $OUTSIDE_TARGETS; do printf 'tests/CMakeFiles/%s.dir ' "$t"; done)
OUTSIDE_DIRS=${OUTSIDE_DIRS% }
EXPECT_CLASS_A_OUTSIDE_SRCS=${EXPECT_CLASS_A_OUTSIDE_SRCS-"tests/qp/test_qp_anti_cycling_arm.cpp tests/qp/test_qp_convex_certificate.cpp"}
EXPECT_CLASS_A_OUTSIDE_UNIVERSE=$(printf '%s' "$EXPECT_CLASS_A_OUTSIDE_SRCS" | wc -w)

# ---- THE DECLARED SECTION SHAPE, PER CONFIGURATION --------------------------
# NC3 (e), MEASURED by the same rehearsal (rehearsal-r2/rehearsal-r2-nc3-sections-{Release,Debug}.txt). The
# non-debug movers are the DERIVED class A library objects (qp/qp_engine, qp/ws_independence) plus the declared class B
# library objects below, each checked unmodified in the range and named in its configuration's class
# B list before the legs. The NOISE-ONLY library-mover slots are declared by name when non-empty.
EXPECT_LIB_NOISE_MOVERS_RELEASE=${EXPECT_LIB_NOISE_MOVERS_RELEASE-""}
EXPECT_LIB_CLASS_B_RELEASE=${EXPECT_LIB_CLASS_B_RELEASE-"CMakeFiles/hven.dir/src/core/ledger.cpp.o CMakeFiles/hven.dir/src/drivers/sqp_solver.cpp.o CMakeFiles/hven.dir/src/globalization/sqp/mixed_lift.cpp.o CMakeFiles/hven.dir/src/globalization/sqp/qn_lift_bridge.cpp.o CMakeFiles/hven.dir/src/globalization/sqp/soc_elastic_restoration.cpp.o CMakeFiles/hven.dir/src/qp/ipqp_engine.cpp.o"}
EXPECT_LIB_CLASS_B_DEBUG=${EXPECT_LIB_CLASS_B_DEBUG-"CMakeFiles/hven.dir/src/core/ledger.cpp.o CMakeFiles/hven.dir/src/drivers/sqp_solver.cpp.o CMakeFiles/hven.dir/src/globalization/sqp/mixed_lift.cpp.o CMakeFiles/hven.dir/src/globalization/sqp/qn_lift_bridge.cpp.o CMakeFiles/hven.dir/src/globalization/sqp/soc_elastic_restoration.cpp.o CMakeFiles/hven.dir/src/qp/ipqp_engine.cpp.o"}
EXPECT_LIB_NOISE_MOVERS_DEBUG=${EXPECT_LIB_NOISE_MOVERS_DEBUG-""}
# DEBUG_ONLY (objects whose bytes differ only in debug information): Release / Debug.
EXPECT_DEBUG_ONLY_RELEASE=${EXPECT_DEBUG_ONLY_RELEASE:-0}
EXPECT_DEBUG_ONLY_DEBUG=${EXPECT_DEBUG_ONLY_DEBUG:-7}

mkdir -p "$G" "$S/logs"
LOG=$S/logs/gates-bps-qc8.log
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
          EXPECT_DEBUG_ONLY_RELEASE EXPECT_DEBUG_ONLY_DEBUG \
          EXPECT_PSYM_B_DEBUG_ONLY EXPECT_CLASS_A_NOISE_ONLY_DEBUG EXPECT_CLASS_A_NOISE_ONLY_RELEASE \
          EXPECT_LIB_NOISE_MOVERS_RELEASE EXPECT_LIB_NOISE_MOVERS_DEBUG \
          EXPECT_DEBUG_TESTS EXPECT_RELEASE_TESTS ADDED_TEST_NAMES \
          EXPECT_REPLAY_DIFFS_WALK EXPECT_REPLAY_DIFFS_SSN EXPECT_REPLAY_DIFFS_IPM EXPECT_REPLAY_DIFFS_TOTAL DECLARED_REPLAY_DIFFS; do
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
n_a_noise_rel=$(printf '%s' "$EXPECT_CLASS_A_NOISE_ONLY_RELEASE" | wc -w)
if [ "$((n_mod - EXPECT_CLASS_A_OUTSIDE_UNIVERSE - n_a_noise_rel))" != "$want_a_rel" ] || [ "$((n_mod - EXPECT_CLASS_A_OUTSIDE_UNIVERSE - n_a_noise_dbg))" != "$want_a_dbg" ]; then
  echo "REFUSED: $n_mod modified .cpp (less $EXPECT_CLASS_A_OUTSIDE_UNIVERSE outside the universe; Release less $n_a_noise_rel and Debug less $n_a_noise_dbg declared NOISE-ONLY), but class A is DECLARED $EXPECT_PSYM_CLASS_A"
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
# NC4 (c): TWO outside targets. For each declared outside source (ADDED or MODIFIED, as declared): it is a
# source of EXACTLY ONE listed target's add_executable block in tests/CMakeLists.txt, and is named exactly once
# in the build files, so its only object is that outside one. No listed target is a P-SYM group.
target_block() { awk -v t="$1" '$0 ~ "add_executable\\("t"([[:space:]]|$)" {f=1} f {print} f && /\)/ {exit}' tests/CMakeLists.txt; }
for t in $OUTSIDE_TARGETS; do
  [ -n "$(target_block "$t")" ] || { echo "REFUSED: no add_executable($t in tests/CMakeLists.txt"; exit 2; }
done
check_outside_source() { # kind (ADDED|MODIFIED) file
  local kind="$1" f="$2" t n_t n_all
  n_t=0
  for t in $OUTSIDE_TARGETS; do
    target_block "$t" | grep -qF "${f#tests/}" && n_t=$((n_t + 1))
  done
  n_all=$(git grep -hF "$(basename "$f")" -- 'CMakeLists.txt' '*/CMakeLists.txt' '*.cmake' | wc -l)
  [ "$n_t" = 1 ] && [ "$n_all" = 1 ] \
    || { echo "REFUSED: $f is a source of $n_t listed outside target(s) and is named $n_all time(s) in the build files, want 1 and 1 -- its object may be captured elsewhere"; exit 2; }
}
for f in $EXPECT_ADDED_OUTSIDE_UNIVERSE; do
  printf '%s\n' $ADDED_CXX | grep -qxF "$f" || { echo "REFUSED: declared outside source $f is not ADDED against $BEFORE"; exit 2; }
  check_outside_source ADDED "$f"
done
for f in $EXPECT_CLASS_A_OUTSIDE_SRCS; do
  printf '%s\n' $MODIFIED_CXX | grep -qxF "$f" || { echo "REFUSED: declared outside source $f is not MODIFIED against $BEFORE"; exit 2; }
  check_outside_source MODIFIED "$f"
done
for d in $OUTSIDE_DIRS; do
  if sed -n '/^PSYM_GROUPS=(/,/^)/p' scripts/psym_compare.sh | grep -qF "$d"; then
    echo "REFUSED: $d IS a P-SYM group -- its objects would be captured"; exit 2
  fi
done
sed -n '/^PSYM_GROUPS=(/,/^)/p' scripts/psym_compare.sh | grep -q 'CMakeFiles' \
  || { echo "REFUSED: could not read PSYM_GROUPS from scripts/psym_compare.sh"; exit 2; }
# The ADDED sources inside the universe are the ADDED ones less the declared outside ones.
# Both are empty this round, and the derivation is still taken rather than short-circuited.
ADDED_IN_UNIVERSE=$(printf '%s\n' $ADDED_CXX | grep -vxF -f <(printf '%s\n' $EXPECT_ADDED_OUTSIDE_UNIVERSE) | tr '\n' ' ')
n_add_u=$(printf '%s' "$ADDED_IN_UNIVERSE" | wc -w)
echo "=== MODIFIED OUTSIDE THE P-SYM UNIVERSE (declared, checked): [$EXPECT_CLASS_A_OUTSIDE_SRCS] -> $OUTSIDE_DIRS"
echo "=== ADDED OUTSIDE THE P-SYM UNIVERSE (declared, checked): [$EXPECT_ADDED_OUTSIDE_UNIVERSE] -> $OUTSIDE_DIRS"
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
# BPS + QC-8 (c2), by value: each declared Release NOISE-ONLY class A object's source IS modified.
for o in $EXPECT_CLASS_A_NOISE_ONLY_RELEASE; do
  src=$(obj_source "$o")
  [ -f "$src" ] || { echo "REFUSED: declared Release NOISE-ONLY class A $o maps to $src, which does not exist"; exit 2; }
  printf '%s\n' $MODIFIED_CXX | grep -qxF "$src" \
    || { echo "REFUSED: declared Release NOISE-ONLY class A $o: its source $src is not MODIFIED in the range"; exit 2; }
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
# F9 (g), by value: every declared Release-only (Debug-skipped) name is a declared ADDED name, with
# no duplicate -- the skip declaration cannot name a test this range did not add.
n_sk=$(skip_added_names | grep -c .); n_sk_u=$(skip_added_names | uniq | grep -c .)
n_sk_out=$(LC_ALL=C comm -23 <(skip_added_names) <(added_names) | grep -c .)
[ "$n_sk" = "$n_sk_u" ] && [ "$n_sk_out" = 0 ] \
  || { echo "REFUSED: the declared Release-only skip list has $n_sk name(s) ($n_sk_u unique), $n_sk_out not a declared ADDED name"; exit 2; }
echo "=== RELEASE-ONLY (Debug GTEST_SKIP) DECLARED: $n_sk name(s), each a declared ADDED name ==="
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
# BPS + QC-8 (c): each RE-DECLARED name is removed by exactly one TEST line AND added by exactly one TEST line in the range, is in neither the
# ADDED nor the REMOVED list, and is present in the baseline (so the by-name set check proves it survives).
n_redecl=$(redeclared_names | grep -c .)
while IFS= read -r t; do
  [ -n "$t" ] || continue
  suite=${t%%.*}; name=${t#*.}
  n_rm=$(git diff "$BEFORE..$HEAD_AT_START" -- 'tests/*.cpp' | grep -E '^-TEST(_P)?\(' | grep -cF "$suite, $name)")
  n_ad=$(git diff "$BEFORE..$HEAD_AT_START" -- 'tests/*.cpp' | grep -E '^\+TEST(_P)?\(' | grep -cF "$suite, $name)")
  n_in_a=$(added_names | grep -cxF -- "$t"); n_in_r=$(removed_names | grep -cxF -- "$t")
  n_bs=$(suite_names "$NAMES_BASE_DIR/suite-release.txt" | grep -cxF -- "$t")
  [ "$n_rm" = 1 ] && [ "$n_ad" = 1 ] && [ "$n_in_a" = 0 ] && [ "$n_in_r" = 0 ] && [ "$n_bs" = 1 ] \
    || { echo "REFUSED: RE-DECLARED $t: removed $n_rm / added $n_ad TEST line(s) (want 1 / 1), in ADDED $n_in_a / REMOVED $n_in_r (want 0 / 0), in the baseline $n_bs time(s) (want 1)"; exit 2; }
done < <(redeclared_names)
[ "$n_removed" = "$((n_rem_names + n_redecl))" ] || { echo "REFUSED: $n_removed TEST line(s) removed in the range; this round declares $n_rem_names REMOVED + $n_redecl RE-DECLARED"; exit 2; }
echo "=== RE-DECLARED (TEST line removed and re-added under the same name): $n_redecl name(s), each removed once and added once, net unchanged by name ==="
for cfg in debug release; do
  bt=$(sed -nE 's/^.*tests passed, [0-9]+ tests failed out of ([0-9]+)$/\1/p' "$NAMES_BASE_DIR/suite-$cfg.txt" | grep -c .)
  [ "$bt" = 1 ] || { echo "REFUSED: the $cfg baseline carries $bt ctest summary lines, want 1"; exit 2; }
  btot=$(sed -nE 's/^.*tests passed, [0-9]+ tests failed out of ([0-9]+)$/\1/p' "$NAMES_BASE_DIR/suite-$cfg.txt")
  if [ $cfg = debug ]; then want=$EXPECT_DEBUG_TESTS; else want=$EXPECT_RELEASE_TESTS; fi
  [ "$want" = "$((btot + n_added - n_rem_names))" ] \
    || { echo "REFUSED: $cfg declared $want != the baseline's $btot + $n_added ADDED - $n_rem_names REMOVED"; exit 2; }
done
echo "=== BY-NAME: baselines $NAMES_BASE_DIR/suite-{debug,release}.txt ($(suite_names "$NAMES_BASE_DIR/suite-debug.txt" | grep -c .) / $(suite_names "$NAMES_BASE_DIR/suite-release.txt" | grep -c .) started names); $n_added ADDED, each added by exactly one TEST line in the range; $n_rem_names REMOVED, each removed by exactly one TEST line; $n_removed TEST lines removed ($n_redecl of them RE-DECLARED)"
# E4 (g), by value: the ADDED non-.cpp files under tests/ EQUAL the declared list, each is
# under the fixture directory, and that directory is named exactly once in the build files
# (the outside target's compile definition), so nothing in the P-SYM universe reads them.
got_files=$(git diff --name-only --diff-filter=A "$BEFORE..$HEAD_AT_START" -- tests/ | grep -v '\.cpp$' | LC_ALL=C sort)
want_files=$(printf '%s\n' "$ADDED_TEST_FILES" | grep -v '^$' | LC_ALL=C sort)
[ "$got_files" = "$want_files" ] \
  || { echo "REFUSED: the ADDED non-.cpp test files [$(printf '%s ' $got_files)] != declared [$(printf '%s ' $want_files)]"; exit 2; }
n_out_fx=$(printf '%s\n' "$want_files" | grep -v '^$' | grep -vc '^tests/qp/fixtures/e2-d4/')
[ "$n_out_fx" = 0 ] || { echo "REFUSED: $n_out_fx declared ADDED test file(s) outside tests/qp/fixtures/e2-d4/"; exit 2; }
# P1E (c): TWO readers now, each naming the directory exactly once in its own target's compile definitions: the walk seam target
# (tests/CMakeLists.txt) and hven_sqp_tests (tests/sqp/CMakeLists.txt). The build files name it exactly twice, and no other way.
n_fx=$(git grep -hF 'fixtures/e2-d4' "$HEAD_AT_START" -- 'CMakeLists.txt' '*/CMakeLists.txt' '*.cmake' | grep -c .)
[ "$n_fx" = 2 ] || { echo "REFUSED: fixtures/e2-d4 is named $n_fx time(s) in the build files, want 2 (the two declared readers)"; exit 2; }
n_fx_blk=$(awk -v t="$FIXTURE_TARGET" '$0 ~ "target_compile_definitions\\("t"([[:space:]]|$)" {f=1} f {print} f && /\)/ {f=0}' tests/CMakeLists.txt | grep -cF 'fixtures/e2-d4')
[ "$n_fx_blk" = 1 ] || { echo "REFUSED: fixtures/e2-d4 is not named in $FIXTURE_TARGET's compile definitions ($n_fx_blk)"; exit 2; }
n_fx_sqp=$(awk '$0 ~ "target_compile_definitions\\(hven_sqp_tests([[:space:]]|$)" {f=1} f {print} f && /\)/ {f=0}' tests/sqp/CMakeLists.txt | grep -cF 'fixtures/e2-d4')
[ "$n_fx_sqp" = 1 ] || { echo "REFUSED: fixtures/e2-d4 is not named in hven_sqp_tests's compile definitions ($n_fx_sqp)"; exit 2; }
# P1E (b): tests/sqp/CMakeLists.txt adds EXACTLY the declared non-comment lines and removes none.
sq_add=$(git diff "$BEFORE..$HEAD_AT_START" -- tests/sqp/CMakeLists.txt | grep -E '^\+[^+]' | sed 's/^+//' | grep -vE '^[[:space:]]*(#|$)' | sed 's/^[[:space:]]*//; s/[[:space:]]*$//' | LC_ALL=C sort | tr '\n' '|')
sq_del=$(git diff "$BEFORE..$HEAD_AT_START" -- tests/sqp/CMakeLists.txt | grep -E '^-[^-]' | sed 's/^-//' | grep -vE '^[[:space:]]*(#|$)' | grep -c .)
sq_want=$(printf '%s\n' "$EXPECT_SQP_CMAKE_ADDS" | grep -v '^$' | LC_ALL=C sort | tr '\n' '|')
[ "$sq_add" = "$sq_want" ] && [ "$sq_del" = 0 ] \
  || { echo "REFUSED: tests/sqp/CMakeLists.txt adds [$sq_add] (declared [$sq_want]) and removes $sq_del (declared 0)"; exit 2; }
echo "=== tests/sqp/CMakeLists.txt (checked): adds exactly the declared $(printf '%s\n' "$EXPECT_SQP_CMAKE_ADDS" | grep -c .) non-comment line(s), removes none"
# QN (b): src/CMakeLists.txt adds EXACTLY the declared library sources (one line each) and the PCH-count assignment, and removes exactly the old assignment.
sr_add=$(git diff "$BEFORE..$HEAD_AT_START" -- src/CMakeLists.txt | grep -E '^\+[^+]' | sed 's/^+//' | grep -vE '^[[:space:]]*(#|$)' | sed 's/^[[:space:]]*//; s/[[:space:]]*$//' | LC_ALL=C sort | tr '\n' '|')
sr_del=$(git diff "$BEFORE..$HEAD_AT_START" -- src/CMakeLists.txt | grep -E '^-[^-]' | sed 's/^-//' | grep -vE '^[[:space:]]*(#|$)' | sed 's/^[[:space:]]*//; s/[[:space:]]*$//' | LC_ALL=C sort | tr '\n' '|')
sr_want_a=$(printf '%s\n' "$EXPECT_SRC_CMAKE_ADDS" | grep -v '^$' | LC_ALL=C sort | tr '\n' '|')
sr_want_d=$(printf '%s\n' "$EXPECT_SRC_CMAKE_DELS" | grep -v '^$' | LC_ALL=C sort | tr '\n' '|')
[ "$sr_add" = "$sr_want_a" ] && [ "$sr_del" = "$sr_want_d" ] \
  || { echo "REFUSED: src/CMakeLists.txt adds [$sr_add] (declared [$sr_want_a]) and removes [$sr_del] (declared [$sr_want_d])"; exit 2; }
# every added library source line is exactly an ADDED src .cpp of the range, and the new count equals the library source count the file asserts.
for f in $(git diff --name-only --diff-filter=A "$BEFORE..$HEAD_AT_START" -- 'src/*.cpp'); do
  printf '%s\n' "$EXPECT_SRC_CMAKE_ADDS" | grep -qxF "${f#src/}" || { echo "REFUSED: ADDED library source $f is not among the declared src/CMakeLists.txt adds"; exit 2; }
done
echo "=== src/CMakeLists.txt (checked): adds exactly the declared $(printf '%s\n' "$EXPECT_SRC_CMAKE_ADDS" | grep -c .) non-comment line(s), removes exactly the declared $(printf '%s\n' "$EXPECT_SRC_CMAKE_DELS" | grep -c .)"
# E4 (d), by value: tests/CMakeLists.txt adds EXACTLY the declared outside sources as
# non-comment lines and removes no non-comment line. NC3 (b): this range ADDS ONE outside source
# (qp/test_qp_saddle_exit.cpp) and tests/CMakeLists.txt adds exactly that line.
cm_add=$(git diff "$BEFORE..$HEAD_AT_START" -- tests/CMakeLists.txt | grep -E '^\+[^+]' | sed 's/^+//' | grep -vE '^[[:space:]]*(#|$)' | sed 's/^[[:space:]]*//; s/[[:space:]]*$//' | LC_ALL=C sort | tr '\n' ' ')
cm_del=$(git diff "$BEFORE..$HEAD_AT_START" -- tests/CMakeLists.txt | grep -E '^-[^-]' | sed 's/^-//' | grep -vE '^[[:space:]]*(#|$)' | grep -c .)
cm_want=$(for f in $EXPECT_ADDED_OUTSIDE_UNIVERSE; do printf '%s\n' "${f#tests/}"; done | LC_ALL=C sort | tr '\n' ' ')
[ "$cm_add" = "$cm_want" ] && [ "$cm_del" = 0 ] \
  || { echo "REFUSED: tests/CMakeLists.txt adds non-comment lines [$cm_add] (declared [$cm_want]) and removes $cm_del (declared 0)"; exit 2; }
echo "=== ADDED TEST FILES (checked): non-.cpp [$(printf '%s ' $got_files)] == declared, under the fixture directory named in the two declared readers' compile definitions ($FIXTURE_TARGET, hven_sqp_tests); tests/CMakeLists.txt adds exactly [$cm_add] and removes no non-comment line"
# R2 (d), by value and BEFORE any leg: the DERIVED clang-format set is NON-EMPTY. G6 passes this
# same derivation to `clang-format --dry-run -Werror`, which reads STDIN when given no file, so an
# empty set would hang the leg instead of passing or failing it. Derived here with the SAME
# pathspec G6's caller uses, so the two cannot disagree.
PREFLIGHT_CHANGED_CXX=$(git diff --name-only "$BEFORE..$HEAD_AT_START" -- 'include/*.h' 'src/*.cpp' 'tests/*.cpp' 'tests/*.h' 'bench/*.cpp' | grep -c .)
[ "$PREFLIGHT_CHANGED_CXX" -gt 0 ] \
  || { echo "REFUSED: the derived clang-format set is EMPTY -- G6 would pass no file to clang-format, which then reads stdin and hangs"; exit 2; }
echo "=== CLANG_FORMAT SET (derived, non-empty): $PREFLIGHT_CHANGED_CXX file(s) ==="
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
ROOT=@ROOT@; S=${GATES_SCRATCH:?}; G=$S/gates
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
ROOT=@ROOT@; S=${GATES_SCRATCH:?}; G=$S/gates
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 ctest --test-dir "$S/gates-dbg" --no-tests=error \
  -R "^Continuation.ProbeBudgetBoundsAFailingProposal$" --output-on-failure > "$G/suite-debug-long.log" 2>&1
rc=$?; tail -8 "$G/suite-debug-long.log"; exit $rc
INNER

# --- G3: Release ----------------------------------------------------------
cat > "$S/g3.sh" <<'INNER'
set -o pipefail
ROOT=@ROOT@; S=${GATES_SCRATCH:?}; G=$S/gates
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
ROOT=@ROOT@; S=${GATES_SCRATCH:?}; G=$S/gates
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
    "M6 CLOSE HEAD-$eng vs M7 W2 BPS QC8 HEAD-$eng (GATED $GATE)" | tee -a "$D/comparison.txt"
  rc=${PIPESTATUS[0]}; echo "  CMP_RC=$rc"
  # THIS ROUND DECLARES DIFFERENCES (E4 (h)), so the comparator's rc 1 is the EXPECTED
  # outcome on every engine and the GATE is the runner's set comparison on this
  # transcript. rc 0 -- no difference at all -- is as undeclared a shape as rc 2
  # (schema mismatch) and is refused here rather than read as success.
  [ $rc -eq 1 ] || { echo "  CMP_RC_UNDECLARED $eng=$rc (this round declares differences; 1 is the declared rc)"; worst=1; }
done
# THE EXACT ARM (QN + QC-1): the previous GREEN record (P1E + C13's gates/replay, whose sources are BEFORE's) compared EXACT, no gate.
# RECORDED, NOT ASSERTED either way: this range claims kExact is untouched (QN is behind hessian_mode, QC-1 claims kExact TRAIN
# field-identical), so rc 0 on every engine is the expectation and any difference is a finding to be reported, never explained away.
PREV=$ROOT/docs/notes/data/2026-10-m7-qn-qc1/gates/replay
: > "$D/exact-vs-qn-qc1-record.txt"
for eng in walk ssn ipm; do
  python3 -B "$ROOT/scripts/compare_replay.py" "$PREV/head-$eng.csv" "$D/head-$eng.csv" \
    "PREVIOUS (QN + QC-1 record 4a0da559) GATE RECORD HEAD-$eng vs M7 BPS QC8 HEAD-$eng (EXACT)" | tee -a "$D/exact-vs-qn-qc1-record.txt"
  echo "  EXACT_RC_$eng=${PIPESTATUS[0]} (RECORDED, not asserted)"
done
exit $worst
INNER

# --- G5: P-SYM, both configs ---------------------------------------------
cat > "$S/g5.sh" <<'INNER'
set -uo pipefail
ROOT=@ROOT@; S=${GATES_SCRATCH:?}; G=$S/gates
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
cp "$ROOT/docs/notes/data/2026-10-m7-bps-qc8/wrappers-gates/classify_psym_m4_uncapped.py" \
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
    nbuilt=0
    for d in $OUTSIDE_DIR; do
      if grep -qF "$d/${f#*/}.o" "$OUT/build-after-$CFG.log"; then
        nbuilt=$((nbuilt + 1))
        echo "ASSERT OK   outside-universe $CFG: $d/${f#*/}.o was BUILT in the after arm"
      fi
    done
    assert_eq "outside-universe $CFG: $f compiled in exactly one listed outside target" "$nbuilt" 1 || rc=1
  done
  n_in=0
  for d in $OUTSIDE_DIR; do
    n_d=$(grep -c "^$d/" "$OUT/after-$CFG/.psym-manifest")
    n_in=$((n_in + n_d))
  done
  assert_eq "outside-universe $CFG: objects of [$OUTSIDE_DIR] in the after manifest" "$n_in" 0 || rc=1
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
  # W1 + CERT1 (b): the classifier used here (classify_psym_m4_uncapped.py) prints its WHOLE negative control, so the carried
  # completeness assertion is back in force exactly as it stood before NC4 r2 relaxed it: printed == stated, or FAIL.
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
      echo "ASSERT FAIL class B asymmetry: $o is a MOVER in Release (or the capped listing did not print it) -- it is in neither the NOISE-ONLY list nor the printed negative control"; rc=1
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
ROOT=@ROOT@; S=${GATES_SCRATCH:?}; G=$S/gates
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
# NC4 (b): the segments carry the placeholder @ROOT@ on their prologue line; the worktree root this file
# derived replaces it here, and assert_segment_scratch then judges what the line RESOLVES to.
for _seg in g1 g2 g3 g4 g5 g6; do
  sed -i "s|^ROOT=@ROOT@;|ROOT=$ROOT;|" "$S/$_seg.sh" || return 1
done
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
# F9 (g): the SKIPPED set by name -- the baseline's Debug skips + the declared Release-only (none this range).
assert_skips "G1 Debug suite" debug "$NAMES_BASE_DIR/suite-debug.txt" "$G/suite-debug.log" || rc1=1

# --- G2: the long test, ALONE ---------------------------------------------
run_locked G2-debug-long bash "$S/g2.sh"; rc2=$?
assert_ctest "G2 long test" "$G/suite-debug-long.log" "$EXPECT_LONG_TESTS" || rc2=1

# --- G3: Release ----------------------------------------------------------
run_locked G3-release bash "$S/g3.sh"; rc3=$?
assert_ctest "G3 Release suite" "$G/suite-release.log" "$EXPECT_RELEASE_TESTS" || rc3=1
assert_names "G3 Release suite" "$G/suite-release.log" "$NAMES_BASE_DIR/suite-release.txt" || rc3=1
# F9 (g): the SKIPPED set by name -- exactly the baseline's Release skips.
assert_skips "G3 Release suite" release "$NAMES_BASE_DIR/suite-release.txt" "$G/suite-release.log" || rc3=1

# --- G4: the replay -------------------------------------------------------
run_locked G4-replay bash "$S/g4.sh" "$REPLAY_RESIDUAL_GATE"; rc4=$?
assert_replay_declared "$G/replay/comparison.txt" "$EXPECT_REPLAY_CELLS" "$EXPECT_REPLAY_COLUMNS" || rc4=1

# --- G5: P-SYM, both configs ---------------------------------------------
run_locked G5-psym bash "$S/g5.sh" "$BEFORE" "$CHANGED_SOURCES" "$CHANGED_HEADERS" \
  "$MIN_NEGATIVE_CONTROL" "$EXPECT_PSYM_DIFFERING" "$EXPECT_PSYM_CLASS_A" \
  "$EXPECT_PSYM_CLASS_B" "$EXPECT_DEBUG_ONLY_RELEASE" "$EXPECT_DEBUG_ONLY_DEBUG" \
  "$EXPECT_LIB_MOVERS_RELEASE" "$EXPECT_LIB_MOVERS_DEBUG" "$ADDED_IN_UNIVERSE" "$EXPECT_LIB_ADDED" \
  "$OUTSIDE_DIRS" "$EXPECT_CLASS_A_OUTSIDE_SRCS $EXPECT_ADDED_OUTSIDE_UNIVERSE" "$EXPECT_PSYM_B_DEBUG_ONLY"; rc5=$?

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
noise_a_src_rel=$(for o in $EXPECT_CLASS_A_NOISE_ONLY_RELEASE; do obj_source "$o"; done | LC_ALL=C sort | tr '\n' ' ')
for CFG in Release Debug; do
  got=$(psym_set "$G/psym/classification-$CFG.txt" A | awk '{print $NF}' | while IFS= read -r o; do obj_source "$o"; done | LC_ALL=C sort | tr '\n' ' ')
  if [ $CFG = Release ]; then want=$(printf '%s\n' $want_a_src | grep -vxF -f <(printf '%s\n' $noise_a_src_rel) | LC_ALL=C sort | tr '\n' ' ')
  else want=$(printf '%s\n' $want_a_src | grep -vxF -f <(printf '%s\n' $noise_a_src) | LC_ALL=C sort | tr '\n' ' '); fi
  if [ -n "$got" ] && [ "$got" = "$want" ]; then
    echo "ASSERT OK   class A by source $CFG: [$got]"
  else
    echo "ASSERT FAIL class A by source $CFG: measured [$got] != derived [$want]"; rc5=1
  fi
done
for o in $EXPECT_CLASS_A_NOISE_ONLY_RELEASE; do
  if grep -qxE "NOISE-ONLY +$o" "$G/psym/compare-Release.txt"; then
    echo "ASSERT OK   Release NOISE-ONLY class A: $o"
  else
    echo "ASSERT FAIL Release NOISE-ONLY class A: $o is not NOISE-ONLY in the Release compare"; rc5=1
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
