################################################################################
# linear/ tests: hven_linear_tests (RS-6a), plus the directory's seam executable and its allocation executable.
# linear/test_symmetric_factor.cpp follows the platform split of its implementation (src/CMakeLists.txt): it exercises MKL Pardiso semantics -- perturbed-pivot evidence, phase-split partial solves --
# that the Accelerate backend answers differently, so it is compiled where that backend is. The DN-0 KKT dumps the dense-arm suite reads (linear/fixtures/dn1/) are named by an absolute path so ctest's working directory is irrelevant.
################################################################################

set(_hven_linear_sources
    test_dense_symmetric_factor.cpp
    test_dense_symmetric_factor_growth.cpp
    test_matching_scaling.cpp
    test_symmetric_factor_dense_arm.cpp
    test_symmetric_factor_evidence_invariants.cpp
    test_symmetric_factor_pardiso_only_options.cpp
)
if(NOT APPLE)
    list(APPEND _hven_linear_sources test_symmetric_factor.cpp)
else()
    hven_test_sources_absent("Apple (MKL-only suite)" test_symmetric_factor.cpp)
endif()

# The former hven_sqp_tests sources: UNIX-only (its subprocess wall-deadline tests drive the
# corpus runner, whose deadline enforcement is fork/exec orchestration, and the suite has never
# been built or derived on Windows; RS-6b is the stage that lifts the gate).
set(_hven_linear_unix_sources
    test_accelerate_probes.cpp
    test_backend_scaffold.cpp
)
if(NOT WIN32)
    list(APPEND _hven_linear_sources ${_hven_linear_unix_sources})
else()
    hven_test_sources_absent("Windows" ${_hven_linear_unix_sources})
endif()

hven_add_test_executable(hven_linear_tests
    SOURCES ${_hven_linear_sources}
    DEFINITIONS
        HVEN_DN1_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/dn1")

################################################################################
# The dense arm's allocation-free hot path (M8 DN-1, T9): its own executable, because the test replaces the global operator new for the binary it links into.
# On Linux it also wraps the C allocator at link time (--wrap: every object in this static link, hven and MKL included), counting in the test's wrappers; see the test's header for what that does not reach.
################################################################################

add_executable(hven_linear_alloc_tests test_dense_arm_allocation.cpp)
_hven_claim_test_sources(hven_linear_alloc_tests test_dense_arm_allocation.cpp)
target_link_libraries(hven_linear_alloc_tests PRIVATE hven::hven GTest::gtest_main)
target_compile_options(hven_linear_alloc_tests PRIVATE ${COMPILE_FLAGS})
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(hven_linear_alloc_tests PRIVATE HVEN_DENSE_ALLOC_WRAP_MALLOC=1)
    target_link_options(hven_linear_alloc_tests PRIVATE
        "LINKER:--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=posix_memalign"
        "LINKER:--wrap=aligned_alloc,--wrap=memalign")
endif()
gtest_discover_tests(hven_linear_alloc_tests DISCOVERY_MODE ${HVEN_GTEST_DISCOVERY_MODE})

################################################################################
# Fault-injection tests (docs/testing.md: the test-seam convention this repository uses for backend fault paths no real fixture can provoke, and for internal state no boundary observation can reach).
# A SEPARATE, standalone executable that recompiles the platform's own backend session TU (pardiso_session.cpp / accelerate_session.cpp) plus the platform's own adapter TU
# (symmetric_factor_mkl.cpp / symmetric_factor_accelerate.cpp) a SECOND time, with HVEN_TESTING defined ONLY on that second copy's compile.
# The MKL session TU carries TWO HVEN_TESTING-gated records -- the guarded parameter-array writes in analyze() (recording whether each write executed, which nothing outside that function can observe),
# and a separate post-pardisoinit snapshot of iparm[10]/iparm[33]/iparm[18] taken before those same entries can be touched by that function's own phase-11 call. Both are sanctioned deviations from this repo's
# boundary-first instrumentation preference and are argued in docs/testing.md; they compile to nothing in a build without HVEN_TESTING, which the not-linking-hven::hven arrangement makes checkable.
# This target does NOT link hven::hven (hven_add_seam_executable): it keeps the production library's compiled adapter object (and every ordinary executable) untouched by HVEN_TESTING, and the link would
# fail anyway (libhven.a and this target's freshly-compiled adapter both define SymmetricFactor's and Factorization's methods).
################################################################################

hven_add_seam_executable(hven_linear_seam_tests
    SOURCES test_fault_injection.cpp
    LIBRARY_SOURCES
        ${HVEN_SRC_DIR}/core/pattern_hash.cpp)
