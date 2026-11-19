################################################################################
# qp/ tests: hven_qp_tests (RS-6a) -- the QP tiers' ordinary suites (walk, SSN, IPQP, the dense oracle, the EQP solve) -- plus the directory's seam executable.
# test_ipqp_acceptance pins the A4 gate artifacts: the committed gate CSVs are named by absolute path (the artifact and the evaluator hold each other in place); the SHIPPED-defaults CSV is the
# placeholder's record and is never regenerated (CLAUDE.md section 7), the second is the same 29 cells at the measured ipqp_init_mu.
################################################################################

set(_hven_qp_sources

)

# The former hven_sqp_tests sources: UNIX-only (its subprocess wall-deadline tests drive the
# corpus runner, whose deadline enforcement is fork/exec orchestration, and the suite has never
# been built or derived on Windows; RS-6b is the stage that lifts the gate).
set(_hven_qp_unix_sources
    test_dense_oracle.cpp
    test_eqp_refine_ab.cpp
    test_eqp_solve.cpp
    test_ipqp_acceptance.cpp
    test_ipqp_certification.cpp
    test_ipqp_counters.cpp
    test_ipqp_engine.cpp
    test_ipqp_kkt_layout.cpp
    test_ipqp_ladder_skip.cpp
    test_ipqp_math.cpp
    test_ipqp_mu0_rule.cpp
    test_ipqp_options.cpp
    test_ipqp_trace.cpp
    test_ipqp_warm_restart.cpp
    test_qp_engine.cpp
    test_qp_engine_border.cpp
    test_qp_engine_indefinite.cpp
    test_qp_engine_tr.cpp
    test_qp_row_contract.cpp
    test_qp_types.cpp
    test_qp_warm_start.cpp
    test_qp_ws_independence.cpp
    test_ssn_engine.cpp
)
if(NOT WIN32)
    list(APPEND _hven_qp_sources ${_hven_qp_unix_sources})
else()
    hven_test_sources_absent("Windows" ${_hven_qp_unix_sources})
endif()

hven_add_test_executable(hven_qp_tests
    SOURCES ${_hven_qp_sources}
    DEFINITIONS
        HVEN_A4_GATE_CSV="${CMAKE_CURRENT_SOURCE_DIR}/../../docs/notes/data/2026-08-m6-w1-acceptance/a4-gate-shipped-defaults.csv"
        HVEN_A4_GATE_MU1E2_CSV="${CMAKE_CURRENT_SOURCE_DIR}/../../docs/notes/data/2026-08-m6-w1-acceptance/a4-gate-mu1e-2.csv")

################################################################################
# QP-tier seam tests (docs/testing.md: the HVEN_TESTING convention, applied to the QP tiers). ONE standalone executable (RS-6a merged the former hven_ipqp_seam_tests and hven_qp_walk_seam_tests and one
# source of the whole-library hven_sqp_seam_tests) that takes the library from hven_testing_library, the HVEN_TESTING build of the whole library compiled once and shared with the solver, ipm and sqp seam executables (RS-6a, Q9: the former narrow closure of fourteen TUs is cheaper as a link against objects that exist anyway, and it retires the narrow-seam flag class); it does not link hven::hven.
# WHAT IT COVERS AND WHY NOTHING ELSE CAN. IPQP: the specification's evidence-failure policy (an InertiaEvidence state other than kObserved permits a step only at a conservative rho floor and downgrades the
# certificate for the whole solve) and the two terminal inertia states the certification read can report (PERTURBED, UNREADABLE) are unreachable from any legal subproblem on this backend; the kUnavailable case is an
# Accelerate path, which under CLAUDE.md section 6's never-fabricate rule stays UNOBSERVED until real Mac hardware runs it. The hook is one function in src/qp/ipqp/ipqp_engine.cpp, at the single line where the tier
# reads KktFactorization::inertia_evidence(): section 6's boundary preference is satisfied with NO deviation (no session file is touched, nothing MPL-derived, the production object is byte-identical with the seam removed).
# WALK: QpEngine has no trace sink; its finest public instrument is one QpCounters per SOLVE. Which BRANCH of the walk a minor took, which constraint and which SIDE, by which drop ROUTE, and what the working SET was
# live inside one iteration of one loop and leave no trace outside it. The hooks are in src/qp/walk/walk_engine.cpp (Apache-2.0, this repository's; no deviation, no notices/ entry).
# SSN: test_dense_solve_wound_seams drives the SSN tier's dense solve gates under a wounded factor (DenseSolveFaultInjector; C341).
# The E2-D4 dump fixtures (qp/support/qp_dump.h loads them) are read in place, absolute so ctest's working directory is irrelevant.
################################################################################

hven_add_seam_executable(hven_qp_seam_tests
    WHOLE_LIBRARY
    SOURCES
        test_dense_solve_wound_seams.cpp
        test_ipqp_seams.cpp
        test_qp_anti_cycling_arm.cpp
        test_qp_ascent_guard.cpp
        test_qp_border_certificate.cpp
        test_qp_certificate_sign.cpp
        test_qp_convex_certificate.cpp
        test_qp_degeneracy_arm.cpp
        test_qp_dense_arm.cpp
        test_qp_flat_face_saddle.cpp
        test_qp_implied_blocker.cpp
        test_qp_latch_reason.cpp
        test_qp_platform_retake.cpp
        test_qp_rank_repair.cpp
        test_qp_refused_exchange.cpp
        test_qp_release_readmission.cpp
        test_qp_saddle_exit.cpp
        test_qp_saddle_shift.cpp
        test_qp_stagnation_stop.cpp
        test_qp_standstill_retreat.cpp
        test_qp_unrun_dependency_test.cpp
        test_qp_walk_seam.cpp
        test_route_reference.cpp
    DEFINITIONS
        HVEN_QP_E2D4_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/e2-d4")
