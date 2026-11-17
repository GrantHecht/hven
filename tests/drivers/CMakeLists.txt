################################################################################
# The shared driver surface's test suite: the types in include/hven/drivers/
# that belong to neither engine -- the status vocabulary today, the result core,
# the shared diagnostics and the console sink as W5 T8 lands them.
#
# A separate executable rather than sources appended to one engine's suite,
# following tests/model/: a shared type tested from inside hven_sqp_tests or
# hven_interior_tests reads as that engine's property. This target links
# hven::hven and nothing engine-specific.
#
# NO unity build, for tests/model/'s reason: anonymous namespaces do not isolate
# across unity-merged translation units, and this suite's files each carry their
# own fixtures. The suite is small and its TUs are cheap.
#
# googletest is already fetched by the parent tests/CMakeLists.txt before this
# directory is added, so this file links GTest::gtest_main directly.
################################################################################

set(HVEN_DRIVERS_TEST_SOURCES
    # M8 AP-5 G3: best-so-far, best_iterate, kStopReturnBest and snapshot() on both
    # engines -- the interface design's T7, T9 and T12 (snapshot).
    test_best_iterate.cpp
    test_callback.cpp
    # The concurrency contract's one pin (M6 W6 T4): N solver instances on N
    # std::threads at common.threads = 1 against the same solves run serially.
    # It lives here rather than in either engine's suite because the claim is
    # about BOTH engines and about the shared common.threads field, and because
    # this target is the one that links hven::hven and nothing engine-specific.
    # Threads::Threads reaches it through hven::hven's own interface.
    test_concurrency.cpp
    test_console_sink.cpp
    # M8 TR-A: the trace-v1 contract (V1-V4, identities, observation changes nothing).
    test_trace_v1_contract.cpp
    # M8 TR-A: the cost contract's structural proof (no event built, no emit called, with no sink).
    test_trace_off_path.cpp
    # M8 TR-B: the exit-door walk (count identity at every exit door of both engines; M7-19).
    test_exit_door_walk.cpp
    # M8 TR-D/TR-D': `history` on both engines == the depth-0 iter stream, always on.
    test_iteration_history.cpp
    # M8 TR-B: the message identities of spec 2.7 on live solves of both engines.
    test_trace_messages.cpp
    # M8 TR-B: the message emit sites held by a source scan, falsified by planted sites.
    test_message_sites.cpp
    # M8 TR-A: the human trace rendered from real solves (goldens under golden/human_trace/).
    test_human_trace_goldens.cpp
    # M8 DN-1 T12/T14: both engines on the dense arm, and the policy's validation and dump.
    test_dense_arm_engines.cpp
    # M8 DN-3: DensePolicy::kAuto at both engines and the QP tiers; the dense_factorizations
    # counters.
    test_dense_auto.cpp
    # AP-2 B5: evaluation failure and the exception contract on both engines --
    # trial-point failures rejected, the start point, foreign exceptions
    # propagated unchanged with last_result() kept, the stop reasons.
    test_evaluation_failure.cpp
    test_options.cpp
    # M7 W0 T5(b): the options dump and its drift pin -- three axes (declared
    # field count, sizeof, dumped line count) against IpmOptions, SqpOptions and
    # the three nested option structs. Here rather than in either engine's suite
    # because the claim is about BOTH engines' printers and the shared
    # CommonOptions block they each flatten.
    test_options_dump.cpp
    # AP-2 B3: the work and time limits, the minor-budget label, the new
    # validation refusals and silent-by-default, on both engines.
    test_limits.cpp
    # AP-2 B7: the absent-bound threshold (CommonOptions::infinite_bound) on both
    # engines, the Nlp and pieces routes and the QP tier.
    test_infinite_bound.cpp
    test_print_tables.cpp
    # AP-2 B2: one problem type -- Problem on both engines, the per-solve re-read,
    # the refusals by name, lambda and g in the problem's row space.
    test_problem.cpp
    test_solve_status.cpp
    test_solve_result.cpp
    # M8 AP-5 G6: IpmCounters, the per-phase diagnostics, IpmSolveRecord::counters.
    test_ipm_counters.cpp
    # M8 AP-5 G1: the stop token (SolveBudget::stop) and StopReason on both engines
    # -- the interface design's T1, T2, T6, T8, T10, T13.
    test_stop_token.cpp
    # M8 AP-5 G2: the recovery-aware stop on the interior-point engine -- the
    # interface design's T5 (IPM half), T9 (IPM half) and T14.
    test_stop_recovery.cpp
    # AP-2 B1: the solvers are movable handles; a move preserves the engine.
    test_solver_move.cpp
    test_threads.cpp
    test_warm_protocol.cpp
)

add_executable(hven_drivers_tests ${HVEN_DRIVERS_TEST_SOURCES})

# The tests tree root joins the path so a cross-suite fixture is named rooted --
# "sqp/support/..." or "interior/..." -- rather than reached by a ../ traversal.
# This directory stays first, so this suite's own headers resolve here.
target_include_directories(hven_drivers_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
# Rooted at this directory, not CMAKE_SOURCE_DIR (see tests/CMakeLists.txt's HVEN_SRC_DIR note).
target_compile_definitions(hven_drivers_tests PRIVATE
    HVEN_SRC_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/../../src"
    HVEN_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden/human_trace")
target_link_libraries(hven_drivers_tests PRIVATE hven::hven GTest::gtest_main)
target_compile_options(hven_drivers_tests PRIVATE ${COMPILE_FLAGS})

# Reduce optimization level for tests to improve compile time, matching the
# other test targets in this tree.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(hven_drivers_tests PRIVATE $<$<CONFIG:Release>:-O1>)
endif()

gtest_discover_tests(hven_drivers_tests DISCOVERY_MODE ${HVEN_GTEST_DISCOVERY_MODE})
