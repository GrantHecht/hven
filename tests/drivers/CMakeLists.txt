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
    test_callback.cpp
    test_console_sink.cpp
    test_options.cpp
    test_solve_status.cpp
    test_solve_result.cpp
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
target_link_libraries(hven_drivers_tests PRIVATE hven::hven GTest::gtest_main)
target_compile_options(hven_drivers_tests PRIVATE ${COMPILE_FLAGS})

# Reduce optimization level for tests to improve compile time, matching the
# other test targets in this tree.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(hven_drivers_tests PRIVATE $<$<CONFIG:Release>:-O1>)
endif()

gtest_discover_tests(hven_drivers_tests DISCOVERY_MODE ${HVEN_GTEST_DISCOVERY_MODE})
