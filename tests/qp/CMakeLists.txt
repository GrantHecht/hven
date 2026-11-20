################################################################################
# qp/ tests: hven_qp_tests (the three QP engines' ordinary suites: walk, SSN, IPQP, the dense
# oracle, the EQP solve) and the directory's seam executable. test_ipqp_acceptance pins the A4 gate
# artifacts: the committed gate CSVs are named by absolute path (the artifact and the evaluator
# hold each other in place); the SHIPPED-defaults CSV is the placeholder's record and is never
# regenerated (CLAUDE.md section 7), the second is the same 29 cells at the measured ipqp_init_mu.
# test_route_reference is the reference-build sentinel for the route pins asserted by the walk
# tests and the seam tests (tests/support/route_reference.h); it sits in the ordinary executable
# because it needs no HVEN_TESTING build.
# Flags: the project's level.
################################################################################

set(_hven_qp_sources)
list(APPEND _hven_qp_sources
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
    test_qp_row_contract.cpp
    test_qp_types.cpp
    test_qp_warm_start.cpp
    test_qp_ws_independence.cpp
    test_route_reference.cpp
    test_ssn_engine.cpp
    test_walk_engine.cpp
    test_walk_engine_border.cpp
    test_walk_engine_indefinite.cpp
    test_walk_engine_tr.cpp
)

hven_add_test_executable(hven_qp_tests
    SOURCES ${_hven_qp_sources}
    INCLUDE_BENCH
    DEFINITIONS
        HVEN_A4_GATE_CSV="${CMAKE_CURRENT_SOURCE_DIR}/../../docs/notes/data/2026-08-m6-w1-acceptance/a4-gate-shipped-defaults.csv"
        HVEN_A4_GATE_MU1E2_CSV="${CMAKE_CURRENT_SOURCE_DIR}/../../docs/notes/data/2026-08-m6-w1-acceptance/a4-gate-mu1e-2.csv")

################################################################################
# QP-tier seam tests (docs/testing.md, "Seam tests"): ONE standalone executable that takes the
# library from hven_testing_library, the HVEN_TESTING build of the whole library compiled once and
# shared with the solver, ipm and sqp seam executables. Its sources are tests/qp/seam/*.cpp.
#   IPQP: the evidence-failure policy (an InertiaEvidence state other than kObserved permits a step
#     only at a conservative rho floor and downgrades the certificate) and the two terminal inertia
#     states the certification read can report (PERTURBED, UNREADABLE) are unreachable from any
#     legal subproblem on this backend. The hook is one function in src/qp/ipqp/ipqp_engine.cpp
#     (no session file, nothing MPL-derived, no deviation). The kUnavailable case is an Accelerate
#     path and stays UNOBSERVED until real Mac hardware runs it (CLAUDE.md section 6).
#   WALK: which BRANCH a minor took, which constraint, which SIDE, by which drop ROUTE, and what the
#     working SET was live inside one iteration and leave no trace outside it. The hooks are in
#     src/qp/walk/walk_engine.cpp (Apache-2.0, this repository's; no notices/ entry).
#   SSN: the dense solve gates under a wounded factor (DenseSolveFaultInjector).
# The E2-D4 dump fixtures (qp/support/qp_dump.h) are read in place, absolute so ctest's working
# directory is irrelevant.
################################################################################

hven_add_seam_executable(hven_qp_seam_tests
    WHOLE_LIBRARY
    SOURCES
        seam/test_dense_solve_wound_seams.cpp
        seam/test_ipqp_seams.cpp
        seam/test_qp_anti_cycling_arm.cpp
        seam/test_qp_ascent_footprint.cpp
        seam/test_qp_ascent_guard.cpp
        seam/test_qp_border_certificate.cpp
        seam/test_qp_certificate_sign.cpp
        seam/test_qp_convex_certificate.cpp
        seam/test_qp_degeneracy_arm.cpp
        seam/test_qp_dense_arm.cpp
        seam/test_qp_flat_face_saddle.cpp
        seam/test_qp_implied_blocker.cpp
        seam/test_qp_latch_reason.cpp
        seam/test_qp_platform_retake.cpp
        seam/test_qp_rank_repair.cpp
        seam/test_qp_refused_exchange.cpp
        seam/test_qp_release_readmission.cpp
        seam/test_qp_saddle_exit.cpp
        seam/test_qp_saddle_shift.cpp
        seam/test_qp_stagnation_stop.cpp
        seam/test_qp_standstill_retreat.cpp
        seam/test_qp_unrun_dependency_test.cpp
        seam/test_qp_walk_seam.cpp
    DEFINITIONS
        HVEN_QP_E2D4_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/e2-d4")
