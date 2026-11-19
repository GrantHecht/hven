################################################################################
# The shared solver surface's test suite, hven_solver_tests (RS-6a): the types in include/hven/solver/ that belong to neither engine -- the status vocabulary, the result core, the shared
# diagnostics -- and the behaviours both engines share (stop token, limits, evaluation failure, threads, dense policy). It links hven::hven and nothing engine-specific.
# NO unity build: anonymous namespaces do not isolate across unity-merged translation units, and this suite's files each carry their own fixtures. The suite is small and its TUs are cheap.
# HVEN_SRC_ROOT is absolute (not CMAKE_SOURCE_DIR: see tests/CMakeLists.txt's HVEN_SRC_DIR note); test_message_sites scans ipm_solver.cpp and sqp_solver.cpp under it.
################################################################################

set(_hven_solver_sources
    test_best_iterate.cpp
    test_callback.cpp
    test_concurrency.cpp
    test_counters_dump.cpp
    test_dense_arm_engines.cpp
    test_dense_auto.cpp
    test_evaluation_failure.cpp
    test_exit_door_walk.cpp
    test_infinite_bound.cpp
    test_iteration_history.cpp
    test_limits.cpp
    test_message_sites.cpp
    test_options.cpp
    test_options_dump.cpp
    test_solve_result.cpp
    test_solver_move.cpp
    test_stop_token.cpp
    test_threads.cpp
)

# Compiled at -O1 in Release, as the retired interior/drivers/model targets compiled them
# (compile time; the sources of the former hven_tests and hven_sqp_tests were not).
set(_hven_solver_o1_sources
    test_best_iterate.cpp
    test_callback.cpp
    test_concurrency.cpp
    test_counters_dump.cpp
    test_dense_arm_engines.cpp
    test_dense_auto.cpp
    test_evaluation_failure.cpp
    test_exit_door_walk.cpp
    test_infinite_bound.cpp
    test_iteration_history.cpp
    test_limits.cpp
    test_message_sites.cpp
    test_options.cpp
    test_options_dump.cpp
    test_solve_result.cpp
    test_solver_move.cpp
    test_stop_token.cpp
    test_threads.cpp
)

hven_add_test_executable(hven_solver_tests
    SOURCES ${_hven_solver_sources}
    O1_SOURCES ${_hven_solver_o1_sources}
    DEFINITIONS
        HVEN_SRC_ROOT="${HVEN_REPO_ROOT}/src")

################################################################################
# Shared-engine seam tests (docs/testing.md; HVEN_TESTING). Both sources drive BOTH engines through the public handles, so this executable recompiles every source of the hven target a SECOND time
# with HVEN_TESTING defined (taken from the target itself) and does not link hven::hven. test_backend_error_seams: the backend-failure half of the exception contract (M8 B5 riders), on the linear
# layer's analyze/factorize injectors. test_stop_poll_seams: the intra-iteration stop polls (M8 AP-5 G4; StopPollSeam, detail/core/stop_poll.h).
################################################################################

hven_add_seam_executable(hven_solver_seam_tests
    WHOLE_LIBRARY
    SOURCES
        test_backend_error_seams.cpp
        test_stop_poll_seams.cpp)
