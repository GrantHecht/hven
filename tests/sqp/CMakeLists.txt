################################################################################
# The SQP engine's own test suite, hven_sqp_tests -- migrated whole at M3 (phase A), laid out by component at RS-2 and reduced at RS-6a to the sources of this directory: the SQP engine's own contract
# (the QP dispatch, the lift oracle, the continuation, the predictor, the quasi-Newton models, the hs battery). Same test names, same GoogleTest fixtures. UNIX-only (see the gate below).
# COMPILE FLAGS: ${COMPILE_FLAGS}, like every other target in this repository (the owner's uniform-flags ruling, CLAUDE.md section 7, executed at phase-C U0).
# Sorted, like every other source list in this tree.
################################################################################

set(_hven_sqp_sources

)

# The former hven_sqp_tests sources: UNIX-only (its subprocess wall-deadline tests drive the
# corpus runner, whose deadline enforcement is fork/exec orchestration, and the suite has never
# been built or derived on Windows; RS-6b is the stage that lifts the gate).
set(_hven_sqp_unix_sources
    test_activity_telemetry.cpp
    test_assembly_eval_seam.cpp
    test_b1_gate.cpp
    test_column_scaling.cpp
    test_continuation.cpp
    test_disproved_entry.cpp
    test_elastic_rung_vehicle.cpp
    test_elastic_witness_start.cpp
    test_equality_basis.cpp
    test_funnel.cpp
    test_hs_battery.cpp
    test_hs_sweeps.cpp
    test_ipqp_dispatch.cpp
    test_lbfgs_damped.cpp
    test_nlp_kkt_check.cpp
    test_predictor.cpp
    test_problem_scaling.cpp
    test_qn_models.cpp
    test_qp_cap_levers.cpp
    test_qp_lift_oracle.cpp
    test_qp_mode_sites.cpp
    test_scale_smoke.cpp
    test_sqp_equality_basis.cpp
    test_sqp_qn_mode.cpp
    test_sqp_restoration.cpp
    test_sqp_solver.cpp
    test_start_point_projection.cpp
    test_trace_messages_sqp.cpp
)
if(NOT WIN32)
    list(APPEND _hven_sqp_sources ${_hven_sqp_unix_sources})
else()
    hven_test_sources_absent("Windows" ${_hven_sqp_unix_sources})
endif()

hven_add_test_executable(hven_sqp_tests
    SOURCES ${_hven_sqp_sources}
    DEFINITIONS
        HVEN_QP_E2D4_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/../qp/fixtures/e2-d4"
        HVEN_SQP_SOLVER_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/../../src/sqp/sqp_solver.cpp"
    TEST_FILTER "-ScaleF7Slow.*")

# The driver TU test_qp_mode_sites.cpp SCANS (M6 W4 T5). It reads the source text -- it does not parse C++ -- and requires every kernel invocation in it to be covered by a nearby `qp.mode` emit or to
# carry an explicit `// trace:` marker, so the schema's per-site list cannot go stale as the driver grows call sites. Absolute, so the test does not depend on ctest's working directory; the test asserts the
# file exists and contains the invocations it expects, so a wrong path fails loudly rather than scanning nothing.
# test_elastic_witness_start.cpp reads the E2-D4 dump fixtures (qp/support/qp_dump.h loads them) in place, absolute as for the QP seam executable.

# TEST_FILTER excludes the ScaleF7Slow suite from CTest REGISTRATION, so the per-commit `ctest` run never executes it. The tests themselves are built and are ordinary gtests; run them directly with
#     ./tests/sqp/hven_sqp_tests --gtest_filter='ScaleF7Slow.*'
# WHEN THEY MUST RUN: in the phase-gate Debug sweep and in Release CI runs of the M3 gate -- the exclusion is from the per-commit cadence, not from the phase gate. See the banners in
# test_scale_smoke.cpp and test_hs_sweeps.cpp for each member's measured cost.

# Continuation.ProbeBudgetBoundsAFailingProposal drives F7CollocationChain(100, ...) through four arms with a cap-pinned QP (1002 minors on the failing proposal) and takes ~780 s in Debug (R6 report,
# task-c-r6-report.md section 4). The default ctest timeout (600 s, the CI Release lane's value) is too small for this one Debug test and produces a spurious Timeout. gtest_discover_tests() tests do not
# exist at CMake-configure time (they are added by a generated script included via the directory's TEST_INCLUDE_FILES property at ctest time), so a plain set_tests_properties() call here would fail to find
# the test. Append a second include file, appended after gtest_discover_tests()'s own, that raises just this one test's TIMEOUT once it has been defined.
# Not under Windows: hven_sqp_tests does not exist there, and the file names a test that would be absent.
if(NOT WIN32)
    set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/continuation_probe_budget_timeout.cmake")
endif()

################################################################################
# SQP seam tests (docs/testing.md: the HVEN_TESTING convention; M7 W2 E1b), and since the M7 W2 ladder round the SQP ladder seam's (hven/detail/sqp/ladder_testing.h, the sixth seam). Both seams
# (the first is hven/detail/sqp/equality_basis_testing.h) sit in sqp_solver.cpp, whose transitive closure is the whole library, so this target recompiles EVERY source of the hven target a SECOND time with
# HVEN_TESTING defined, taking hven's own sources, include directories and compile definitions from the target itself (derived, not carried), and does not link hven::hven: libhven.a's objects and these
# define the same symbols.
################################################################################

hven_add_seam_executable(hven_sqp_seam_tests
    WHOLE_LIBRARY
    SOURCES
        test_sqp_equality_basis_seams.cpp
        test_sqp_ladder_seams.cpp)
