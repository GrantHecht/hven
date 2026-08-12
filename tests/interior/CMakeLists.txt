################################################################################
# Interior-point engine (ex-PSIOPT) test suite — migrated as-is from
# psiopt/tests/ (hven M2 milestone, engine-move task). Same test names,
# same GoogleTest fixtures; only the include paths were mechanically
# retargeted onto include/hven/.
#
# A separate executable, not folded into hven_tests: it mirrors psiopt's own
# separately-buildable test binary, keeps this migration's diff reviewable on
# its own, and does not disturb hven_tests' existing HVEN_TEST_SOURCES list or
# discovery-mode logic. googletest is already fetched by the parent
# tests/CMakeLists.txt (FetchContent_MakeAvailable(googletest) runs before
# add_subdirectory(interior) below), so this file links GTest::gtest_main
# directly rather than re-declaring the fetch.
################################################################################

set(HVEN_INTERIOR_TEST_SOURCES
    test_acceptance_diagnostics.cpp
    test_classic_merit_restoration.cpp
    test_filter_acceptance.cpp
    test_funnel_acceptance.cpp
    test_jet_mkl_guard.cpp
    test_kkt_factorization.cpp
    test_l1_restoration.cpp
    test_merit_rules.cpp
    test_monitored_governor.cpp
    test_nlp_adapter.cpp
    test_nlp_multiplier_seeding.cpp
    test_nlp_solver.cpp
    test_proximal_restoration.cpp
    test_recovery_dispatch_gate.cpp
    test_switching_acceptance.cpp
    test_watchdog.cpp
)

add_executable(hven_interior_tests ${HVEN_INTERIOR_TEST_SOURCES})
target_include_directories(hven_interior_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(hven_interior_tests PRIVATE hven::hven GTest::gtest_main)
target_compile_options(hven_interior_tests PRIVATE ${COMPILE_FLAGS})

# Unity build -- merge test TUs, same pattern (and batch size) as
# psiopt/tests/CMakeLists.txt's psiopt_tests. This is load-bearing, not just
# a compile-time optimization: test_nlp_adapter.cpp (and others) only
# #include nlp_adapter.h, which forward-declares NonLinearProgram, and rely
# on another test TU in the same unity batch to have already pulled in the
# complete definition (non_linear_program.h) -- exactly as they did in
# psiopt/tests. Splitting this into separate TUs is a source-level fix
# (adding the missing include) that AS-IS migration does not make; carried
# over unchanged, see the migration report.
set_target_properties(hven_interior_tests PROPERTIES
    UNITY_BUILD ON
    UNITY_BUILD_BATCH_SIZE 8
)

# Reduce optimization level for tests to improve compile time, matching
# psiopt/tests/CMakeLists.txt and hven's own test targets.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(hven_interior_tests PRIVATE $<$<CONFIG:Release>:-O1>)
endif()

gtest_discover_tests(hven_interior_tests DISCOVERY_MODE ${HVEN_GTEST_DISCOVERY_MODE})
