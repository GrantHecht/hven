################################################################################
# solver/ tests: hven_solver_tests: the types in include/hven/solver/ that belong to neither engine
# (the status vocabulary, the result core, the shared diagnostics) and the behaviours both engines
# share (stop token, limits, evaluation failure, threads, dense policy). Links hven::hven and
# nothing engine-specific. HVEN_SRC_ROOT is absolute (see tests/CMakeLists.txt's HVEN_SRC_DIR
# note); test_message_sites scans ipm_solver.cpp and sqp_solver.cpp under it.
# Flags: RELEASE_O1 (the directory rule in tests/CMakeLists.txt).
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

hven_add_test_executable(hven_solver_tests
    SOURCES ${_hven_solver_sources}
    RELEASE_O1
    DEFINITIONS
        HVEN_SRC_ROOT="${HVEN_REPO_ROOT}/src")

################################################################################
# Shared-engine seam tests (docs/testing.md, "Seam tests"). Both sources drive BOTH engines through
# the public handles, so the executable takes the whole library from hven_testing_library and does
# not link hven::hven. test_backend_error_seams: the backend-failure half of the exception
# contract, on the linear layer's analyze/factorize injectors. test_stop_poll_seams: the
# intra-iteration stop polls (StopPollSeam, detail/core/stop_poll.h).
################################################################################

hven_add_seam_executable(hven_solver_seam_tests
    WHOLE_LIBRARY
    SOURCES
        seam/test_backend_error_seams.cpp
        seam/test_stop_poll_seams.cpp)
