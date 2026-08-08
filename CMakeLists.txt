cmake_minimum_required(VERSION 3.24)
project(hven VERSION 0.1.0 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_MESSAGE_LOG_LEVEL "STATUS")

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

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there. You may need to remove CMakeCache.txt.")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    message(FATAL_ERROR "You cannot build hven with the Microsoft Compiler. Please use Clang or GCC and try again.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
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
# Code Directories
################################################################################

include(GNUInstallDirs)

add_library(hven STATIC)
add_library(hven::hven ALIAS hven)

target_include_directories(hven PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Consumers of hven::hven need Eigen types and the sparse-backend link flags,
# so these are linked PUBLIC rather than PRIVATE.
target_link_libraries(hven PUBLIC Eigen3::Eigen fmt::fmt-header-only Threads::Threads)
if(APPLE)
    target_link_libraries(hven PUBLIC AccelerateSparse::AccelerateSparse)
else()
    target_include_directories(hven PUBLIC ${INCLUDE_DIRS})
    target_link_libraries(hven PUBLIC ${LINK_LIBS})
endif()

target_compile_options(hven PRIVATE ${COMPILE_FLAGS})

# Sources come from src/CMakeLists.txt via target_sources(hven PRIVATE ...).
add_subdirectory(src)

if(HVEN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
