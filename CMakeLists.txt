cmake_minimum_required(VERSION 3.24)
project(hven VERSION 0.1.0 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

################################################################################
# Submodule helpers
#
# cmake/git-submodule-init.cmake and cmake/git-submodule-update.cmake (both
# migrated verbatim from the origin project) define the `git-submodule-init` /
# `git-submodule-update` convenience targets (run manually, e.g. `ninja
# git-submodule-update`, after a fresh clone). The FATAL_ERROR checks below
# tell a fresh clone exactly what command closes the gap when dep/eigen or
# dep/fmt is missing — the same pattern the origin project's root
# CMakeLists.txt uses for its own dep/ submodules.
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
# The two sizing knobs the engine's headers/sources read directly (the QP
# thread default and the arena size) are NOT defined here at directory
# scope. They are usage requirements of the hven target -- one of them is
# read by a PUBLIC header -- so they are attached to the target itself,
# further down, where a consumer inherits them by linking rather than by
# happening to be configured under this directory.
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
    # defined but no NOMINMAX. None of this was hit in the origin project
    # because its own include environment never manifested the
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
#
# Every entry here is wrapped in $<BUILD_INTERFACE:...>: Eigen3::Eigen and
# fmt::fmt-header-only are ALIASes of local (non-exported) INTERFACE targets,
# and INCLUDE_DIRS/LINK_LIBS/AccelerateSparse::AccelerateSparse resolve to
# absolute paths captured on THIS build machine (MKL's find_library() hits,
# the Apple SDK's framework search). None of that belongs baked into
# hvenTargets.cmake -- install(EXPORT) would either reject the unexported
# ALIAS targets outright or silently pin an install-tree consumer to this
# machine's MKL location. BUILD_INTERFACE keeps all of it build-tree-only;
# the install/export block at the end of this file re-attaches every one of
# them for install-tree consumers via cmake/hvenConfig.cmake.in, which must
# be kept in sync with whatever is wrapped here.
target_link_libraries(hven PUBLIC
    $<BUILD_INTERFACE:Eigen3::Eigen>
    $<BUILD_INTERFACE:fmt::fmt-header-only>
    $<BUILD_INTERFACE:Threads::Threads>
)
target_include_directories(hven PUBLIC $<BUILD_INTERFACE:${INCLUDE_DIRS}>)
target_link_libraries(hven PUBLIC $<BUILD_INTERFACE:${LINK_LIBS}>)
if(APPLE)
    target_link_libraries(hven PUBLIC $<BUILD_INTERFACE:AccelerateSparse::AccelerateSparse>)
endif()

# Usage requirements a consumer's own translation units need regardless of
# whether they compile hven's Eigen/fmt-touching public headers via
# add_subdirectory or an installed hven::hven -- these are plain macros, not
# paths, so unlike the block above they are safe (and necessary) to bake
# directly into the exported target rather than route through BUILD_INTERFACE
# + config-file re-attachment. FMT_HEADER_ONLY was previously reached only by
# linking the fmt::fmt-header-only ALIAS (now BUILD_INTERFACE-only above);
# the other three were previously directory-scope add_compile_definitions()
# calls above, which apply to hven's own sources and anything add_subdirectory
# ()'d under this directory (src/, tests/) but never to an external consumer's
# translation units.
target_compile_definitions(hven PUBLIC
    FMT_HEADER_ONLY
    FMT_USE_LOCALE=0
    EIGEN_INITIALIZE_MATRICES_BY_ZERO
    EIGEN_DONT_PARALLELIZE
)

# Platform/backend macros that hven's PUBLIC headers themselves branch on --
# these change struct LAYOUT, not just codegen, so an install-tree consumer
# that omits them gets a different sizeof/offsetof than the library was built
# with. Audited by tracing the transitive #include closure of every header
# under include/hven/ that is NOT under include/hven/detail/ (the public
# surface) for #ifdef/#ifndef/#if defined() on anything hven's own build
# defines (as opposed to compiler/OS builtins like __APPLE__/_WIN32, which
# the consumer's own compiler already supplies correctly):
#
#   - USE_ACCELERATE_SPARSE gates two double members and two setter
#     declarations directly on PSIOPT's settings struct
#     (include/hven/drivers/psiopt.h) -- a real ABI-layout hazard on Apple.
#   - NOMINMAX / WIN32_LEAN_AND_MEAN / _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR
#     are needed for include/hven/detail/interior/utils/thread_pool.h to
#     even COMPILE on Windows (it is reached from the public
#     include/hven/drivers/non_linear_program.h); without them a Windows
#     consumer hits the exact <windows.h> max/min collision and
#     constexpr-mutex-constructor failure this project's own build works
#     around for itself (see the if(WIN32) add_compile_definitions() block
#     above).
#
# ACCELERATE_NEW_LAPACK / HVEN_USE_ACCELERATE_LAPACK were checked and are
# NOT exported: the only header that requires them
# (hven/detail/linear/lapacke_shim.h) is reached exclusively from
# src/linear/dense_symmetric_factor.cpp, never from any header a consumer
# includes. Likewise MKL_LP64 (mkl_types.h only branches on MKL_ILP64, which
# hven never defines, so MKL_INT is `int` regardless) and the MKL_INT-using
# headers (pardiso_session.h, fault_injection.h, accelerate_session.h) are
# .cpp-only, never reached from a public header either.
if(APPLE)
    target_compile_definitions(hven PUBLIC USE_ACCELERATE_SPARSE)
endif()
if(WIN32)
    target_compile_definitions(hven PUBLIC
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR
    )
endif()

# The alignment Eigen assumes for its own heap allocations and fixed-size
# vectorized types (EIGEN_MAX_ALIGN_BYTES) is the actual ABI-layout quantity
# behind the heap corruption scripts/check_install_smoke.sh found live: it is
# derived from whichever SIMD ISA macros (__AVX512F__/__AVX__/__AVX2__/...)
# are defined at the point <Eigen/Core> is included, which SIMD_FLAGS (below)
# controls for hven's own sources but says nothing about a consumer's TU.
# Codegen flags (-march/-mtune/-ffast-math/the inline-threshold pair) are
# NOT re-exported here -- they affect instruction selection, not layout, and
# exporting them would (a) force -march=native to re-evaluate on the
# consumer's own host, which gives NO cross-machine guarantee and can
# reproduce the exact corruption class this is fixing if the consumer's CPU
# supports a narrower ISA than the build machine's, (b) silently flip a
# strict-FP consumer to fast-math, and (c) collide with any consumer that
# sets its own -inline-threshold (an unwrapped "-mllvm"/"-inline-threshold=N"
# pair split by CMake's option de-duplication is a hard clang error the
# moment the consumer's value differs from hven's). Resolved by an actual
# compile+run against hven's own SIMD_FLAGS (cmake/probes/
# eigen_max_align_bytes.cpp) rather than duplicating Eigen's ISA-to-alignment
# table here, so it tracks Eigen's own logic exactly and can never drift.
#
# Invoked as a direct compiler call (execute_process), not try_compile/
# try_run: try_compile's generated CMakeLists.txt unconditionally forwards
# the CALLING scope's CURRENT CMAKE_CXX_FLAGS (which cmake/FindMKL.cmake
# mutates via a bare `set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DMKL_LP64
# ${ABI}")`, above) with a plain set() that silently shadows any override
# passed through CMAKE_FLAGS "-DCMAKE_CXX_FLAGS=..." -- confirmed live: that
# form of this probe compiled with "-DMKL_LP64 -m64" only and reported 16
# regardless of SIMD_FLAGS, i.e. it never saw -march=native at all. Calling
# the compiler directly sidesteps try_compile's variable-forwarding rules
# entirely and uses exactly and only the flags listed here.
execute_process(
    COMMAND "${CMAKE_CXX_COMPILER}" -std=c++20 ${SIMD_FLAGS}
        -I "${CMAKE_CURRENT_SOURCE_DIR}/dep/eigen"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/probes/eigen_max_align_bytes.cpp"
        -o "${CMAKE_CURRENT_BINARY_DIR}/hven_align_probe"
    RESULT_VARIABLE _hven_align_compile_result
    OUTPUT_VARIABLE _hven_align_compile_output
    ERROR_VARIABLE _hven_align_compile_output
)
if(NOT _hven_align_compile_result EQUAL 0)
    message(FATAL_ERROR
        "hven: failed to compile the EIGEN_MAX_ALIGN_BYTES probe (see "
        "cmake/probes/eigen_max_align_bytes.cpp) with SIMD_FLAGS='${SIMD_FLAGS}': "
        "${_hven_align_compile_output}")
endif()
execute_process(
    COMMAND "${CMAKE_CURRENT_BINARY_DIR}/hven_align_probe"
    RESULT_VARIABLE _hven_align_run_result
    OUTPUT_VARIABLE HVEN_EIGEN_MAX_ALIGN_BYTES
)
if(NOT _hven_align_run_result EQUAL 0 OR NOT HVEN_EIGEN_MAX_ALIGN_BYTES MATCHES "^[0-9]+$")
    message(FATAL_ERROR
        "hven: failed to run the EIGEN_MAX_ALIGN_BYTES probe (exit "
        "${_hven_align_run_result}, output '${HVEN_EIGEN_MAX_ALIGN_BYTES}'). "
        "This value is required to export a correct ABI contract to hven's "
        "consumers -- see cmake/probes/eigen_max_align_bytes.cpp.")
endif()
unset(_hven_align_compile_result)
unset(_hven_align_compile_output)
unset(_hven_align_run_result)
message(STATUS
    "hven: EIGEN_MAX_ALIGN_BYTES resolved to ${HVEN_EIGEN_MAX_ALIGN_BYTES} "
    "for the active SIMD flags")
# PUBLIC (not INTERFACE-only): forcing this on hven's own sources too is a
# no-op (it is exactly the value Eigen would have auto-derived from the same
# SIMD_FLAGS) and removes any risk of the exported value silently drifting
# from what hven's own build actually used.
target_compile_definitions(hven PUBLIC
    EIGEN_MAX_ALIGN_BYTES=${HVEN_EIGEN_MAX_ALIGN_BYTES})

target_compile_options(hven PRIVATE ${COMPILE_FLAGS})

# HVEN_VERSION_STRING follows the origin project's version-string-on-the-
# command-line idiom: it makes src/core/version.cpp forward the single source
# of truth (this file's `project()` VERSION) instead of carrying its own hardcoded
# copy, and gives tests/core/test_version.cpp the same macro to compare
# against so the test actually guards against the two drifting apart.
target_compile_definitions(hven PRIVATE HVEN_VERSION_STRING="${PROJECT_VERSION}")

# The engine's two build-supplied sizing knobs.
#
# HVEN_DEFAULT_QP_THREADS is PUBLIC because two PUBLIC headers read it
# directly -- include/hven/drivers/psiopt.h (the qp_threads_ default member
# initializer) and include/hven/drivers/optimization_problem_base.h -- so a
# consumer that includes them without the definition would fail on an
# undefined identifier inside a header it did not write. Carried as a usage
# requirement, linking hven::hven is all a consumer has to do.
#
# HVEN_DEFAULT_ARENA_SIZE is PRIVATE: the only reader is
# src/interior/utils/memory_management.cpp, an hven source, and nothing in
# any header names it.
target_compile_definitions(hven
    PUBLIC HVEN_DEFAULT_QP_THREADS=8
    PRIVATE HVEN_DEFAULT_ARENA_SIZE=256)

# Sources come from src/CMakeLists.txt via target_sources(hven PRIVATE ...).
add_subdirectory(src)

if(HVEN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

################################################################################
# Install / export
#
# No PROJECT_IS_TOP_LEVEL guard: hven's primary consumer (tycho) pulls this
# project in via add_subdirectory() and needs these same rules to fire as
# part of ITS OWN `cmake --install` so libhven.a, hven's headers, and hven's
# own export files land inside tycho's install tree alongside tycho's -- that
# is the only way a `find_package(tycho)` install-tree consumer ends up able
# to link the solver symbols tycho's headers declare. A consumer that wants
# hven on its own still gets a normal install by configuring hven/ as the
# top-level project (its CMakeLists.txt already tests that way -- see
# cmake/git-submodule-*.cmake and the FATAL_ERROR guards above, which run
# the same regardless of who is the top-level project).
#
# Only the `hven` target itself is installed/exported. hven_dep_eigen and
# hven_dep_fmt (the local INTERFACE targets ALIASed to Eigen3::Eigen /
# fmt::fmt-header-only) are deliberately NOT part of hvenTargets: every link
# to them on the `hven` target above is BUILD_INTERFACE-wrapped, so they
# never appear in the exported target's usage requirements in the first
# place, and install(EXPORT) never has to reason about exporting an ALIAS of
# a target that is itself un-exported.
################################################################################

include(CMakePackageConfigHelpers)

# No INCLUDES DESTINATION here: the `hven` target already declares its own
# $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}> include directory (see
# target_include_directories(hven PUBLIC ...) above) -- adding INCLUDES
# DESTINATION on top of that duplicated the same path twice in the generated
# hvenTargets.cmake's INTERFACE_INCLUDE_DIRECTORIES. One source of truth.
install(TARGETS hven
    EXPORT hvenTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/hven
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

# Eigen and fmt are vendored submodules with no install rules of their own;
# hven publishes them under the standard Eigen3::Eigen / fmt::fmt-header-only
# names (see the hven_dep_eigen/hven_dep_fmt block above), so a self-contained
# install needs their headers too -- flattened directly into
# CMAKE_INSTALL_INCLUDEDIR, the same include root hven's own headers land in,
# so hvenConfig.cmake.in can reconstruct both as IMPORTED targets purely from
# PACKAGE_PREFIX_DIR without any consumer-side find_package(Eigen3) /
# find_package(fmt). This mirrors tycho's own root CMakeLists.txt precedent
# for the identical situation (its bundled-Eigen / bundled-fmt install
# blocks) -- chosen over find_dependency(Eigen3)/find_dependency(fmt) because
# neither is a real, independently-installed package here: they are vendored
# submodule checkouts with no upstream Config file, so find_dependency would
# require a consumer to have their OWN Eigen3/fmt CMake packages installed,
# which is not this project's vendoring contract on the add_subdirectory side
# either.
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/dep/eigen/Eigen/Core")
    message(FATAL_ERROR
        "Bundled Eigen not found at dep/eigen/. "
        "Run: git submodule update --init dep/eigen")
endif()
install(DIRECTORY dep/eigen/Eigen
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY dep/eigen/unsupported
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/dep/fmt/include/fmt/format.h")
    message(FATAL_ERROR
        "Bundled fmt not found at dep/fmt/. "
        "Run: git submodule update --init dep/fmt")
endif()
install(DIRECTORY dep/fmt/include/fmt
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# MKL and AccelerateSparse are real system/SDK dependencies, not vendored
# ones -- unlike Eigen/fmt above, a consumer must independently have MKL (or,
# on Apple, the Accelerate framework) available, exactly as an
# add_subdirectory() consumer already must. hvenConfig.cmake.in re-finds
# whichever one hven itself was built against via find_dependency(), which
# needs these two custom Find modules (neither MKL nor AccelerateSparse ships
# a real upstream CMake package) on the consumer's module path -- install
# them alongside the rest of the package config, the same pattern tycho's own
# root CMakeLists.txt uses for the identical re-find.
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindMKL.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindAccelerateSparse.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hven
)

if(APPLE)
    set(HVEN_SPARSE_SOLVER "AccelerateSparse")
else()
    set(HVEN_SPARSE_SOLVER "MKL")
endif()

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/hvenConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# PATH_VARS CMAKE_INSTALL_INCLUDEDIR makes @PACKAGE_CMAKE_INSTALL_INCLUDEDIR@
# available in hvenConfig.cmake.in, computed relative to the relocated
# install prefix rather than the hardcoded literal "include" -- a consumer
# that installs hven with -DCMAKE_INSTALL_INCLUDEDIR=<something else> still
# gets a config file whose EXISTS checks (Eigen/fmt/hven headers) point at
# where the headers actually landed.
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/hvenConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/hvenConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hven
    PATH_VARS CMAKE_INSTALL_INCLUDEDIR
)

install(EXPORT hvenTargets
    FILE hvenTargets.cmake
    NAMESPACE hven::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hven
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/hvenConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/hvenConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hven
)
