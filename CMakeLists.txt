cmake_minimum_required(VERSION 3.24)
project(hven VERSION 0.1.0 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

################################################################################
# Submodule helpers
#
# cmake/git-submodule-init.cmake and cmake/git-submodule-update.cmake (both
# migrated verbatim from tycho) define the `git-submodule-init` /
# `git-submodule-update` convenience targets (run manually, e.g. `ninja
# git-submodule-update`, after a fresh clone). The FATAL_ERROR checks below
# tell a fresh clone exactly what command closes the gap when dep/eigen or
# dep/fmt is missing — the same pattern tycho's root CMakeLists.txt uses for
# its own dep/ submodules.
################################################################################

include(cmake/git-submodule-init.cmake)
include(cmake/git-submodule-update.cmake)

if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    message(FATAL_ERROR "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there. You may need to remove CMakeCache.txt.")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    message(FATAL_ERROR "You cannot build hven with the Microsoft Compiler. Please use Clang or GCC and try again.")
endif()

# Needed before CMAKE_INSTALL_INCLUDEDIR is referenced by any target below
# (both the vendored-dependency interface targets and the hven target itself
# use it in their INSTALL_INTERFACE genex).
include(GNUInstallDirs)

# CMAKE_CXX_STANDARD / CMAKE_CXX_STANDARD_REQUIRED are set once, by
# hven_compile_options() below — not duplicated here, so there is a single
# owner for those two settings. CMAKE_CXX_EXTENSIONS is not touched by that
# macro, so it is owned here instead.
set(CMAKE_CXX_EXTENSIONS OFF)

################################################################################
############### Options #########################################################
################################################################################

option(HVEN_BUILD_TESTS "Build the hven C++ unit test suite (requires Google Test via FetchContent)" ON)
option(HVEN_BUILD_BENCH "Build the hven benchmark suite" OFF)
option(HVEN_BUILD_BINDINGS "Build the hven Python bindings" OFF)

# bench/ and bindings/ are empty scaffolding directories today (no
# CMakeLists.txt yet) — fail loudly rather than silently no-op if someone
# turns the option on before the subdirectory exists.
if(HVEN_BUILD_BENCH)
    message(FATAL_ERROR "HVEN_BUILD_BENCH=ON but bench/ has no CMakeLists.txt yet — not yet available.")
endif()
if(HVEN_BUILD_BINDINGS)
    message(FATAL_ERROR "HVEN_BUILD_BINDINGS=ON but bindings/ has no CMakeLists.txt yet — not yet available.")
endif()

################################################################################
############### Build Settings ###################################################
################################################################################

set(LINK_TIME_OPT FALSE)          ### DO LTO - Recommended for full release
set(CLANG_MAX_INLINE_DEPTH 225)

# Floating-point optimization policy.
# STRICT: no fast-math relaxations
# SAFER_FAST: all fast-math optimizations except finite-math-only
# FAST: full fast-math
set(_HVEN_FP_MODE_DEFAULT "SAFER_FAST")

set(HVEN_FP_MODE "${_HVEN_FP_MODE_DEFAULT}" CACHE STRING
    "Floating-point optimization mode. Valid values: STRICT, SAFER_FAST, FAST."
)
set_property(CACHE HVEN_FP_MODE PROPERTY STRINGS STRICT SAFER_FAST FAST)

# Normalize to uppercase for comparison without FORCE-writing back to the cache,
# so that ccmake / cmake-gui edits are not silently overwritten on re-configure.
string(TOUPPER "${HVEN_FP_MODE}" HVEN_FP_MODE)

set(_HVEN_VALID_FP_MODES STRICT SAFER_FAST FAST)
list(FIND _HVEN_VALID_FP_MODES "${HVEN_FP_MODE}" _HVEN_FP_MODE_INDEX)
if(_HVEN_FP_MODE_INDEX EQUAL -1)
    message(FATAL_ERROR
        "Invalid HVEN_FP_MODE='${HVEN_FP_MODE}'. Valid values: STRICT, SAFER_FAST, FAST."
    )
endif()

set(_HVEN_WINDOWS_CLANG_NO_FINITE_MATH_ONLY_FLAG "-fno-finite-math-only")
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
   AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    # clang-cl ignores raw GNU-style frontend flags; forward them explicitly.
    set(_HVEN_WINDOWS_CLANG_NO_FINITE_MATH_ONLY_FLAG "/clang:-fno-finite-math-only")
endif()

if(WIN32 AND HVEN_FP_MODE STREQUAL "SAFER_FAST")
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR
            "HVEN_FP_MODE=SAFER_FAST requires Clang on Windows to disable finite-math-only."
        )
    endif()

    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag(
        "${_HVEN_WINDOWS_CLANG_NO_FINITE_MATH_ONLY_FLAG}"
        HVEN_HAS_NO_FINITE_MATH_ONLY_FLAG
    )
    if(NOT HVEN_HAS_NO_FINITE_MATH_ONLY_FLAG)
        message(FATAL_ERROR
            "The active Windows Clang frontend does not support "
            "${_HVEN_WINDOWS_CLANG_NO_FINITE_MATH_ONLY_FLAG}, so "
            "HVEN_FP_MODE=SAFER_FAST is unavailable."
        )
    endif()
endif()

# Auto-detect ccache to accelerate repeated builds.
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    message(STATUS "ccache found: ${CCACHE_PROGRAM}")
endif()

################################################################################
##########################  Set Compiler Flags  ################################
################################################################################

# SIMD arch, release/debug, LTO, FP-mode, sanitizer, and inline-threshold
# flag assembly lives in cmake/hven_compile_options.cmake (hven_compile_options()).
# This is also where CMAKE_CXX_STANDARD / CMAKE_CXX_STANDARD_REQUIRED get set
# (see the single-owner comment above).
include(cmake/hven_compile_options.cmake)
hven_compile_options()

message(STATUS "Floating-point mode: ${HVEN_FP_MODE}")
if(HVEN_FP_MODE STREQUAL "STRICT")
    message(STATUS "IEEE 754 compliance: STRICT (no fast-math relaxations)")
elseif(HVEN_FP_MODE STREQUAL "SAFER_FAST")
    message(STATUS "IEEE 754 compliance: SAFER_FAST (fast-math except finite-math-only)")
else()
    message(STATUS "IEEE 754 compliance: FAST (full fast-math)")
endif()

if(UNIX AND NOT APPLE)
    # MKL's threading layer is Intel's libiomp5; the compile flag matches the
    # runtime library resolved by hven_resolve_sparse_backend() below.
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        list(APPEND COMPILE_FLAGS "-fopenmp=libiomp5")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GCC|GNU")
        list(APPEND COMPILE_FLAGS "-fopenmp")
    endif()
endif()

################################################################################
###################### Find and set our dependencies ###########################
################################################################################

foreach(_hven_dep eigen fmt)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/dep/${_hven_dep}/CMakeLists.txt")
        message(FATAL_ERROR
            "Bundled ${_hven_dep} not found at dep/${_hven_dep}/. "
            "Run: git submodule update --init dep/${_hven_dep}")
    endif()
endforeach()

# Eigen — header-only, vendored at dep/eigen. Exposed as a SYSTEM interface
# target so Eigen's own warnings do not pollute consumer builds.
add_library(hven_dep_eigen INTERFACE)
target_include_directories(hven_dep_eigen SYSTEM INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/dep/eigen>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
add_library(Eigen3::Eigen ALIAS hven_dep_eigen)

# fmt — header-only, vendored at dep/fmt. FMT_HEADER_ONLY avoids building
# fmt's compiled-library sources (dep/fmt/CMakeLists.txt is not added).
add_library(hven_dep_fmt INTERFACE)
target_include_directories(hven_dep_fmt SYSTEM INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/dep/fmt/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_definitions(hven_dep_fmt INTERFACE FMT_HEADER_ONLY)
add_library(fmt::fmt-header-only ALIAS hven_dep_fmt)

# Set up external dependencies
find_package(Threads REQUIRED)

# Apple Accelerate vs. Intel MKL resolution, plus the resulting
# INCLUDE_DIRS/LINK_LIBS, lives in cmake/hven_sparse_backend.cmake
# (hven_resolve_sparse_backend()). It hard FATAL_ERRORs when the platform
# backend cannot be located.
include(cmake/hven_sparse_backend.cmake)
hven_resolve_sparse_backend()

if(UNIX)
    list(APPEND LINK_LIBS m)
endif()

################################################################################
# Preprocessor definitions the interior-point engine (ex-PSIOPT, migrated in
# under src/drivers, src/model, src/interior) assumes were set project-wide by
# its origin project's own root CMakeLists.txt. Applied here, at directory
# scope, so every target configured below (hven itself, hven_tests, the
# interior-engine test executable, and golden_rig) sees the same preprocessor
# state the engine's own numerics depend on — most load-bearingly the first
# two, which the origin project's build comments mark "NEVER EVER GET RID OF
# THIS EVER": Eigen matrices left uninitialized (the release-build default)
# can silently carry NaN/garbage into a computation instead of a
# reproducible zero, and EIGEN_DONT_PARALLELIZE keeps Eigen's own internal
# threading from contending with the engine's explicit thread pool.
add_compile_definitions(EIGEN_INITIALIZE_MATRICES_BY_ZERO)
add_compile_definitions(EIGEN_DONT_PARALLELIZE)
# fmt is already FMT_HEADER_ONLY (hven_dep_fmt, above); this additionally
# matches the origin project's -DFMT_USE_LOCALE=0 so the engine's
# fmt-based printing keeps the same (locale-independent) formatting
# behavior it had there.
add_compile_definitions(FMT_USE_LOCALE=0)
# Sizing knobs the engine's headers/sources read directly (jet.h's QP
# thread default, memory_management.h's arena size). These are NOT
# renamed to an HVEN_ prefix: they are supplied by the build, not defined
# in any moved file (see the migration report's mechanical-edit-class
# audit for the distinction that rule follows).
add_compile_definitions(TYCHO_DEFAULT_QP_THREADS=8)
add_compile_definitions(TYCHO_DEFAULT_ARENA_SIZE=256)
if(WIN32)
    # clang-cl against the MSVC STL: std::mutex's constexpr default
    # constructor trips a known STL/compiler interaction the engine's
    # thread_pool.h (many std::mutex members) then hits at compile time.
    add_compile_definitions(_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR)

    # VERIFIED header chain that pulls <windows.h> into the migrated
    # interior-point engine's translation units, unguarded (no NOMINMAX
    # defined anywhere before this fix): include/hven/detail/interior/
    # utils/thread_pool.h itself does `#elif defined(_WIN32) #include
    # <windows.h>` (its Windows thread-naming/affinity path), and that
    # header is pulled in transitively by non_linear_program.h -> psiopt.h
    # -> every driver TU, so windows.h's own `max`/`min` function-like
    # macros are live for the rest of each such translation unit --
    # exactly the classic collision with the `std::max`/`std::min` calls
    # further down in thread_pool.h itself and in non_linear_program.h,
    # barrier_math.h, inertia_regularization.h, and others reached the
    # same way. Two further, independent sites, same pattern, found by a
    # follow-up sweep of every <windows.h>/_WIN32 site in the moved tree:
    # src/interior/utils/get_core_count.cpp includes <Windows.h> directly
    # (its GetLogicalProcessorInformation path) and
    # src/interior/utils/color_text.cpp includes <Windows.h> directly (its
    # SetConsoleMode path) -- both with WIN32_LEAN_AND_MEAN locally
    # defined but no NOMINMAX. psiopt never hit any of this inside tycho
    # because tycho's own include environment never manifested the
    # collision (a different header-inclusion order/set) -- it is exposed
    # here, not introduced here. NOMINMAX/WIN32_LEAN_AND_MEAN are
    # compile-definition-only
    # (edit-class (d)): the moved sources are not touched, and defining
    # NOMINMAX before ANY <windows.h> inclusion in a TU is exactly the
    # documented, standard fix for this exact collision. No pre-existing
    # hven file (core/, linear/) includes <windows.h> at all (grepped
    # clean), so narrowing what windows.h itself declares
    # (WIN32_LEAN_AND_MEAN, which get_core_count.cpp already opts into
    # locally) costs nothing here; hven uses none of the subsystems it
    # excludes (winsock, COM, GDI, ...).
    add_compile_definitions(NOMINMAX)
    add_compile_definitions(WIN32_LEAN_AND_MEAN)
endif()

################################################################################
# Code Directories
################################################################################

add_library(hven STATIC)
add_library(hven::hven ALIAS hven)

target_include_directories(hven PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# The C++20 requirement TRAVELS WITH THE TARGET, not just with this directory.
# hven_compile_options() sets CMAKE_CXX_STANDARD, which governs targets defined
# under this project and nothing else: a parent project that does
# add_subdirectory(hven) and links hven::hven from a target of ITS own gets the
# headers without the standard, and include/hven/linear/symmetric_factor.h
# needs <concepts>. Below C++20 that consumer fails to compile, inside a header
# it did not write, for a reason its own CMakeLists never mentions. A PUBLIC
# compile feature makes the requirement part of the target's usage
# requirements, so a consumer is raised to C++20 instead of being diagnosed at
# the preprocessor.
target_compile_features(hven PUBLIC cxx_std_20)

# Consumers of hven::hven need Eigen types and the sparse-backend link flags,
# so these are linked PUBLIC rather than PRIVATE. INCLUDE_DIRS/LINK_LIBS are
# applied on every platform (on Apple, hven_resolve_sparse_backend() leaves
# them as just Threads::Threads + ${CMAKE_DL_LIBS} since MKL_* is never
# populated there) so there is one link block, not a platform-asymmetric one;
# AccelerateSparse::AccelerateSparse is added on top for Apple specifically.
target_link_libraries(hven PUBLIC Eigen3::Eigen fmt::fmt-header-only Threads::Threads)
target_include_directories(hven PUBLIC ${INCLUDE_DIRS})
target_link_libraries(hven PUBLIC ${LINK_LIBS})
if(APPLE)
    target_link_libraries(hven PUBLIC AccelerateSparse::AccelerateSparse)
endif()

target_compile_options(hven PRIVATE ${COMPILE_FLAGS})

# HVEN_VERSION_STRING mirrors tycho's -DTYCHO_VERSIONSTRING="${CMAKE_PROJECT_VERSION}"
# idiom: it makes src/core/version.cpp forward the single source of truth
# (this file's `project()` VERSION) instead of carrying its own hardcoded
# copy, and gives tests/core/test_version.cpp the same macro to compare
# against so the test actually guards against the two drifting apart.
target_compile_definitions(hven PRIVATE HVEN_VERSION_STRING="${PROJECT_VERSION}")

# Sources come from src/CMakeLists.txt via target_sources(hven PRIVATE ...).
add_subdirectory(src)

if(HVEN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
