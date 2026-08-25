################################################################################
# The model contract surface's test suite: the provider-side contract types in
# include/hven/model/ that a consumer meets before any engine is in the picture
# -- the claim arenas and their published tables, the declaration and its
# boundary validation, the structural key, the structure epoch's ordering and
# failure-restore rules, the identity probe, and the request flags that select
# what one evaluation call produces. It also carries the NlpModel bridge, the
# contract's second provider, which is engine-free in the same sense.
#
# A separate executable rather than sources appended to hven_tests: it keeps
# this suite's own support fixture (a fake aggregate implemented against the
# contract alone) out of hven_tests' unity/discovery arrangement, and it makes
# "what does the contract guarantee without an engine?" one runnable target.
#
# NO unity build here, deliberately. Anonymous namespaces do not isolate across
# unity-merged translation units, and this suite's files each carry their own
# fixtures with deliberately parallel roles; merging them would couple files
# that are meant to be readable one at a time. The suite is small and its TUs
# are cheap.
#
# googletest is already fetched by the parent tests/CMakeLists.txt before this
# directory is added, so this file links GTest::gtest_main directly.
################################################################################

set(HVEN_MODEL_TEST_SOURCES
    support/fake_aggregate.cpp
    test_aggregate_contract.cpp
    test_contract_flags.cpp
    test_identity_probe.cpp
    test_model_structure_key.cpp
    test_model_surface_kkt.cpp
    test_nlp_model_aggregate.cpp
    test_structure_epoch.cpp
)

add_executable(hven_model_tests ${HVEN_MODEL_TEST_SOURCES})

# Two roots on the include path, and the second one is why: the bridge's
# equivalence pins consume the HS transcriptions that live beside the SQP suite
# (tests/sqp/support/hs_problems.h, header-only test support). The tests tree
# root joins the path so that fixture is named rooted -- "sqp/support/..." --
# rather than reached by a ../ traversal from a source file. This directory
# itself stays first, so a quoted include of this suite's own support/ headers
# resolves here.
target_include_directories(hven_model_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(hven_model_tests PRIVATE hven::hven GTest::gtest_main)
target_compile_options(hven_model_tests PRIVATE ${COMPILE_FLAGS})

# Reduce optimization level for tests to improve compile time, matching the
# other test targets in this tree.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(hven_model_tests PRIVATE $<$<CONFIG:Release>:-O1>)
endif()

gtest_discover_tests(hven_model_tests DISCOVERY_MODE ${HVEN_GTEST_DISCOVERY_MODE})
