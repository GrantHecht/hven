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
#
# -Wno-character-conversion is itself a version-dependent flag: clang only
# grew a -Wcharacter-conversion diagnostic group at some point after LLVM
# 20 (confirmed on hven's own Windows CI runner, clang-cl reporting "Clang
# 20.1.8 with MSVC-like command-line" -- the flag is unrecognized there and,
# because gtest's own /WX is already active on these two targets, an
# unrecognized -Wno-* is promoted from clang's usual
# -Wunknown-warning-option *warning* to a hard error, failing the build
# outright rather than just losing the suppression). check_cxx_compiler_flag
# probes for it first so the suppression is applied only where the compiler
# actually understands it; CMAKE_REQUIRED_FLAGS=-Werror during the probe is
# the standard belt-and-suspenders for this exact clang gotcha (some CMake
# versions already promote -Wunknown-warning-option internally for
# Clang-family compilers when probing a -Wno-* flag, but not all did before
# this project's own CMake floor, so the probe does not rely on that).
if(WIN32)
    include(CheckCXXCompilerFlag)
    # Save/restore rather than unconditionally unset: harmless today since
    # nothing sets CMAKE_REQUIRED_FLAGS earlier in this file, but the
    # save/restore idiom costs nothing and does not silently start
    # clobbering a caller's value if this probe is ever hoisted or reused
    # somewhere CMAKE_REQUIRED_FLAGS is already in use.
    set(_hven_saved_required_flags "${CMAKE_REQUIRED_FLAGS}")
    set(CMAKE_REQUIRED_FLAGS "-Werror")
    check_cxx_compiler_flag("-Wno-character-conversion" HVEN_HAS_NO_CHARACTER_CONVERSION_FLAG)
    set(CMAKE_REQUIRED_FLAGS "${_hven_saved_required_flags}")
    unset(_hven_saved_required_flags)
    if(NOT HVEN_HAS_NO_CHARACTER_CONVERSION_FLAG)
        message(STATUS
            "gtest/gtest_main: this clang does not recognize the "
            "-Wcharacter-conversion diagnostic group -- skipping the "
            "-Wno-character-conversion suppression (nothing to suppress if the "
            "compiler has no such warning to raise in the first place; not "
            "independently verified here, so if gtest-printers.h's "
            "char8_t->char32_t conversion does still fail /WX under a "
            "diagnostic with a different name on this compiler, that will "
            "surface as a build failure right after this message rather than "
            "silently).")
    endif()
    foreach(_t gtest gtest_main)
        if(TARGET ${_t} AND HVEN_HAS_NO_CHARACTER_CONVERSION_FLAG)
            target_compile_options(${_t} PRIVATE -Wno-character-conversion)
        endif()
    endforeach()
endif()

include(GoogleTest)

# gtest_discover_tests() defaults to DISCOVERY_MODE POST_BUILD: it runs the
# freshly-linked test binary as part of the ninja build graph itself, right
# after linking, to enumerate its cases. That makes a plain `ninja`/`cmake
# --build` non-hermetic with respect to the runtime environment -- the build
# graph now depends on the binary being *runnable*, not just producible, and
# CMake has no reach into what environment that build-time run inherits (it
# is whatever the invoking job's process environment already has). On
# Windows this is a real problem: hven links MKL's OpenMP runtime
# (libiomp5md.lib), which is an import stub for a DLL with no static
# equivalent on Windows -- so hven_tests.exe genuinely cannot load without
# the oneAPI redist bin directories on PATH (see
# .github/workflows/ci.yml's windows-clang-release job for where PATH gets
# set), and a build that runs before PATH is correct fails inside ninja's
# own output rather than at `ctest`, which is a confusing place to have to
# diagnose it. DISCOVERY_MODE PRE_TEST defers discovery to `ctest`
# invocation time instead (a generated wrapper runs discovery immediately
# before the tests execute), which decouples "the build produced an
# artifact" from "the artifact can run" -- a PATH/DLL problem then surfaces
# as a Test-step failure with ctest's own diagnostics, not a build failure.
# Linux and macOS do not have this problem in the first place -- FindMKL.cmake
# bakes MKL's directory into CMAKE_BUILD_RPATH on Linux, and Accelerate is a
# system framework on macOS -- so POST_BUILD (the default, and the simpler,
# more immediate option when nothing forces the change) stays in place
# there; only Windows switches.
set(HVEN_GTEST_DISCOVERY_MODE POST_BUILD)
if(WIN32)
    set(HVEN_GTEST_DISCOVERY_MODE PRE_TEST)
endif()

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

gtest_discover_tests(hven_tests DISCOVERY_MODE ${HVEN_GTEST_DISCOVERY_MODE})

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

gtest_discover_tests(hven_fault_injection_tests DISCOVERY_MODE ${HVEN_GTEST_DISCOVERY_MODE})

################################################################################
# The golden-numerics rig: the harness, the seam adapters, the traces, the
# expected-table machinery, and the consumed-surface audit tooling. Its native
# arms need nothing beyond this repository and run here on every build; its two
# old-seam arms are built only when their sibling checkout is named. See
# tests/golden_rig/CMakeLists.txt and docs/testing.md's rig section.
################################################################################

add_subdirectory(golden_rig)
