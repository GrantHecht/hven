################################################################################
# hven unit tests — Google Test via FetchContent (same pattern as tycho's
# tests/cpp/CMakeLists.txt and psiopt/tests/CMakeLists.txt).
################################################################################

include(FetchContent)

# EXCLUDE_FROM_ALL is a CMake >= 3.28 FetchContent_Declare keyword; this
# project's floor is cmake_minimum_required(VERSION 3.24) (root
# CMakeLists.txt), so it is omitted here rather than silently accepted-but-
# ineffective on 3.24-3.27. The cost is that googletest's own targets
# (gtest, gtest_main, gmock, gmock_main) become part of the default `all`
# target instead of being pulled in only transitively; googletest's own
# sample/test executables stay off by default regardless (gtest_build_tests /
# gtest_build_samples default OFF upstream), so this only means gmock/
# gmock_main compile even though hven_tests below links gtest_main only.
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.17.0
    GIT_SHALLOW    TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF)   # keep gtest out of any future cmake --install of hven
FetchContent_MakeAvailable(googletest)

# Clang with MSVC headers: suppress char8_t->char32_t implicit conversion
# warning in gtest-printers.h. gtest compiles with -WX so this becomes an
# error without the suppression. Mirrors tycho's tests/cpp/CMakeLists.txt.
if(WIN32)
    foreach(_t gtest gtest_main)
        if(TARGET ${_t})
            target_compile_options(${_t} PRIVATE -Wno-character-conversion)
        endif()
    endforeach()
endif()

include(GoogleTest)

set(HVEN_TEST_SOURCES
    core/test_version.cpp
    core/test_sparse_backend_link.cpp
    core/test_pattern_hash.cpp
    linear/test_dense_symmetric_factor.cpp
    # Evidence-honesty invariants written once against the public API and
    # checked on whichever backend this build targets -- see
    # docs/testing.md and the file's own header comment. Deliberately NOT
    # gated by platform, unlike test_symmetric_factor.cpp below.
    linear/test_symmetric_factor_evidence_invariants.cpp
    # Options::ordering / Options::weighted_matching throw-path coverage:
    # internally platform-gated (like test_fault_injection.cpp), so it
    # compiles into hven_tests on every platform and asserts the throw on
    # Accelerate, the inverse guard on MKL.
    linear/test_symmetric_factor_pardiso_only_options.cpp
)

# SymmetricFactor's MKL-specific tests follow the same platform split as its
# implementation (src/CMakeLists.txt): the suite exercises MKL Pardiso
# semantics — perturbed-pivot evidence, phase-split partial solves — that the
# Accelerate backend answers differently, so it is compiled where that
# backend is.
if(NOT APPLE)
    list(APPEND HVEN_TEST_SOURCES linear/test_symmetric_factor.cpp)
endif()

add_executable(hven_tests ${HVEN_TEST_SOURCES})
target_link_libraries(hven_tests PRIVATE hven::hven GTest::gtest_main)
target_compile_options(hven_tests PRIVATE ${COMPILE_FLAGS})

# Same single source of truth as the hven target itself (root CMakeLists.txt)
# — see src/core/version.cpp and core/test_version.cpp.
target_compile_definitions(hven_tests PRIVATE HVEN_VERSION_STRING="${PROJECT_VERSION}")

gtest_discover_tests(hven_tests)

################################################################################
# Fault-injection tests (docs/testing.md: the test-seam convention this repo
# uses for backend fault paths no real fixture can provoke, and for internal
# state no boundary observation can reach). A SEPARATE, standalone executable
# that recompiles the platform's own backend session TU
# (pardiso_session.cpp / accelerate_session.cpp) plus the platform's own
# adapter TU (symmetric_factor_mkl.cpp / symmetric_factor_accelerate.cpp) a
# SECOND time, with HVEN_TESTING defined ONLY on that second copy's compile.
#
# The MKL session TU carries exactly ONE HVEN_TESTING-gated record -- two
# lines at the guarded parameter-array writes in analyze(), recording whether
# each write executed, which nothing outside that function can observe. It is
# the single sanctioned deviation from this repo's boundary-first
# instrumentation preference and is argued in docs/testing.md; it compiles to
# nothing here, which the same not-linking-hven::hven arrangement below makes
# checkable. This target does NOT link hven::hven:
#   - it is what keeps the production `hven` library's own compiled adapter
#     object (and hven_tests, which links it) completely untouched by
#     HVEN_TESTING -- byte-for-byte what it would be if this target and
#     fault_injection.h did not exist;
#   - it is also required for the link to succeed at all: both this
#     target's freshly-compiled adapter TU and libhven.a's own define
#     hven::linear::SymmetricFactor's and Factorization's methods, which
#     would be a duplicate-symbol error if both were linked into one binary.
################################################################################

add_executable(hven_fault_injection_tests
    ${CMAKE_SOURCE_DIR}/src/core/pattern_hash.cpp
    linear/test_fault_injection.cpp
)

if(NOT APPLE)
    target_sources(hven_fault_injection_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src/linear/pardiso_session.cpp
        ${CMAKE_SOURCE_DIR}/src/linear/symmetric_factor_mkl.cpp
    )
else()
    target_sources(hven_fault_injection_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src/linear/accelerate_session.cpp
        ${CMAKE_SOURCE_DIR}/src/linear/symmetric_factor_accelerate.cpp
    )
endif()

# HVEN_TESTING is target-wide (not per-source-file): the injectors and the
# observer in hven/detail/linear/fault_injection.h must be visible to the
# adapter TU that guards its one call site with `#ifdef HVEN_TESTING`, to the
# MKL session TU that records the two did-the-write-execute flags under the
# same guard, and to this target's own test_fault_injection.cpp, which reads
# and resets them. No other target ever defines HVEN_TESTING -- see the block
# comment above.
target_compile_definitions(hven_fault_injection_tests PRIVATE HVEN_TESTING=1)

target_include_directories(hven_fault_injection_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(hven_fault_injection_tests PRIVATE
    Eigen3::Eigen fmt::fmt-header-only Threads::Threads GTest::gtest_main
)
target_include_directories(hven_fault_injection_tests PRIVATE ${INCLUDE_DIRS})
target_link_libraries(hven_fault_injection_tests PRIVATE ${LINK_LIBS})
if(APPLE)
    target_link_libraries(hven_fault_injection_tests PRIVATE AccelerateSparse::AccelerateSparse)
endif()
target_compile_options(hven_fault_injection_tests PRIVATE ${COMPILE_FLAGS})

gtest_discover_tests(hven_fault_injection_tests)

################################################################################
# The golden-numerics rig: the harness, the seam adapters, the traces, the
# expected-table machinery, and the consumed-surface audit tooling. Its native
# arms need nothing beyond this repository and run here on every build; its two
# old-seam arms are built only when their sibling checkout is named. See
# tests/golden_rig/CMakeLists.txt and docs/testing.md's rig section.
################################################################################

add_subdirectory(golden_rig)
