################################################################################
# linear/ tests: hven_linear_tests, the directory's seam executable and its allocation executable.
# test_symmetric_factor exercises MKL Pardiso semantics (perturbed-pivot evidence, phase-split
# partial solves) that the Accelerate backend answers differently, so it is built where that
# backend is. The DN-0 KKT dumps the dense-arm suite reads (fixtures/dn1/) are named by an absolute
# path so ctest's working directory is irrelevant.
# Flags: the project's level.
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
list(APPEND _hven_linear_sources
    test_accelerate_probes.cpp
    test_backend_scaffold.cpp
    test_sparse_backend_link.cpp
)

hven_add_test_executable(hven_linear_tests
    SOURCES ${_hven_linear_sources}
    DEFINITIONS
        HVEN_DN1_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/dn1")

################################################################################
# The dense arm's allocation-free hot path: its own executable, because the test replaces the
# global operator new for the binary it links into. On Linux it also wraps the C allocator at link
# time (--wrap: every object in this static link, hven and MKL included), counting in the test's
# wrappers; the test's header says what that does not reach.
################################################################################

set(_hven_alloc_link_options "")
set(_hven_alloc_definitions "")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(_hven_alloc_definitions HVEN_DENSE_ALLOC_WRAP_MALLOC=1)
    set(_hven_alloc_link_options
        "LINKER:--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=posix_memalign"
        "LINKER:--wrap=aligned_alloc,--wrap=memalign")
endif()
hven_add_isolated_test_executable(hven_linear_alloc_tests
    SOURCES test_dense_arm_allocation.cpp
    DEFINITIONS ${_hven_alloc_definitions}
    LINK_OPTIONS ${_hven_alloc_link_options})

################################################################################
# Fault-injection tests: the HVEN_TESTING convention (docs/testing.md, "Seam tests"), for backend
# fault paths no real fixture can provoke and internal state no boundary observation can reach.
# A standalone executable that recompiles the platform's backend session TU plus its adapter TU
# with HVEN_TESTING defined; it does NOT link hven::hven (that keeps the production objects
# untouched, and libhven.a would define the same symbols twice). The MKL session TU carries two
# HVEN_TESTING-gated records, both sanctioned deviations argued in docs/testing.md.
################################################################################

hven_add_seam_executable(hven_linear_seam_tests
    SOURCES seam/test_fault_injection.cpp
    LIBRARY_SOURCES
        ${HVEN_SRC_DIR}/core/pattern_hash.cpp)
