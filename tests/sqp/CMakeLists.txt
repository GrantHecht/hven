################################################################################
# sqp/ tests: hven_sqp_tests: the SQP engine's own contract (the QP dispatch, the lift oracle, the
# continuation, the predictor, the quasi-Newton models, the HS battery) and the directory's seam
# executable.
# Flags: the project's level.
################################################################################

set(_hven_sqp_sources)
list(APPEND _hven_sqp_sources
    test_activity_telemetry.cpp
    test_assembly_eval_seam.cpp
    test_backend_analysis_tally.cpp
    test_caller_step_limit_and_qp_dump.cpp
    test_column_scaling.cpp
    test_continuation.cpp
    test_disproved_entry.cpp
    test_elastic_rung_vehicle.cpp
    test_elastic_witness_start.cpp
    test_equality_basis_driver.cpp
    test_equality_basis_unit.cpp
    test_funnel.cpp
    test_hs_battery.cpp
    test_hs_sweeps.cpp
    test_i19_radius_cycle.cpp
    test_ipqp_dispatch.cpp
    test_iteration_telemetry.cpp
    test_lbfgs_damped.cpp
    test_nlp_kkt_check.cpp
    test_predictor.cpp
    test_problem_scaling.cpp
    test_qn_models.cpp
    test_qp_cap_levers.cpp
    test_qp_lift_oracle.cpp
    test_qp_mode_sites.cpp
    test_scale_smoke.cpp
    test_sqp_qn_mode.cpp
    test_sqp_restoration.cpp
    test_sqp_solver.cpp
    test_start_point_projection.cpp
    test_trace_messages_sqp.cpp
    test_tu0_levers.cpp
    test_warm_ingest_complementarity.cpp
)

hven_add_test_executable(hven_sqp_tests
    SOURCES ${_hven_sqp_sources}
    DEFINITIONS
        HVEN_QP_E2D4_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/../qp/fixtures/e2-d4"
        HVEN_SQP_SOLVER_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/../../src/sqp/sqp_solver.cpp"
    TEST_FILTER "-ScaleF7Slow.*")

# test_qp_mode_sites SCANS the SQP solver TU (HVEN_SQP_SOLVER_SOURCE, absolute so the test does not
# depend on ctest's working directory). It reads source text, does not parse C++, and requires
# every kernel invocation in it to be covered by a nearby `qp.mode` emit or to carry an explicit
# `// trace:` marker; the test asserts the file exists and holds the invocations it expects, so a
# wrong path fails loudly rather than scanning nothing. test_elastic_witness_start reads the E2-D4
# dump fixtures in place, absolute as for the QP seam executable.

# TEST_FILTER excludes the ScaleF7Slow suite from ctest REGISTRATION, so the per-commit `ctest` run
# never executes it. The tests are built and ordinary gtests; run them directly with
#     ./tests/sqp/hven_sqp_tests --gtest_filter='ScaleF7Slow.*'
# They must run in the phase-gate Debug sweep and in Release CI runs of the M3 gate; see the
# banners in test_scale_smoke.cpp and test_hs_sweeps.cpp for each member's measured cost.

# Continuation.ProbeBudgetBoundsAFailingProposal takes ~780 s in Debug, above the default ctest
# timeout. gtest_discover_tests() tests do not exist at configure time (a generated script defines
# them at ctest time), so set_tests_properties() here would fail to find the test: append a second
# include file, after gtest_discover_tests()'s own, that raises that one test's TIMEOUT once it is
# defined.
set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/continuation_probe_budget_timeout.cmake")

################################################################################
# SQP seam tests (docs/testing.md, "Seam tests"): the equality-basis seam and the ladder seam
# (hven/detail/sqp/equality_basis_testing.h, ladder_testing.h), both in sqp_solver.cpp, whose
# transitive closure is the whole library, so the executable takes the library from
# hven_testing_library and does not link hven::hven.
################################################################################

hven_add_seam_executable(hven_sqp_seam_tests
    WHOLE_LIBRARY
    SOURCES
        seam/test_sqp_equality_basis_seams.cpp
        seam/test_sqp_ladder_seams.cpp)
