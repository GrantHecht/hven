################################################################################
# hven unit tests — Google Test via FetchContent.
################################################################################

include(FetchContent)

# Fail early and legibly when Google Test cannot be fetched. FetchContent's own
# failure is a git-clone error deep in a sub-build; this names the switch that
# makes the network unnecessary. Skipped when googletest is already available
# without a download: a populated cache, or a user-supplied source directory.
set(_hven_gtest_url "https://github.com/google/googletest.git")
set(_hven_gtest_tag "v1.17.0")
if(NOT FETCHCONTENT_SOURCE_DIR_GOOGLETEST
   AND NOT EXISTS "${FETCHCONTENT_BASE_DIR}/googletest-src/CMakeLists.txt")
    set(_hven_gtest_reachable FALSE)
    if(NOT FETCHCONTENT_FULLY_DISCONNECTED AND NOT FETCHCONTENT_UPDATES_DISCONNECTED)
        find_package(Git QUIET)
        if(GIT_FOUND)
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" ls-remote --exit-code "${_hven_gtest_url}" "${_hven_gtest_tag}"
                RESULT_VARIABLE _hven_gtest_probe_result
                OUTPUT_QUIET ERROR_QUIET
                TIMEOUT 30)
            if(_hven_gtest_probe_result EQUAL 0)
                set(_hven_gtest_reachable TRUE)
            endif()
        else()
            # No git to probe with; let FetchContent try (and report) itself.
            set(_hven_gtest_reachable TRUE)
        endif()
    endif()
    if(NOT _hven_gtest_reachable)
        message(FATAL_ERROR
            "hven: HVEN_BUILD_TESTS=ON needs Google Test (${_hven_gtest_tag}) from "
            "${_hven_gtest_url}, which could not be reached (no network?). "
            "Configure with -DHVEN_BUILD_TESTS=OFF to build hven without its test suite, "
            "or point -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=<path> at a local googletest checkout.")
    endif()
endif()

# EXCLUDE_FROM_ALL is a CMake >= 3.28 FetchContent_Declare keyword; this
# project's floor is cmake_minimum_required(VERSION 3.24) (root
# CMakeLists.txt), so it is omitted here rather than silently accepted-but-
# ineffective on 3.24-3.27. The cost is that googletest's own targets
# (gtest, gtest_main, gmock, gmock_main) become part of the default `all`
# target instead of being pulled in only transitively; googletest's own
# sample/test executables stay off by default regardless (gtest_build_tests /
# gtest_build_samples default OFF upstream), so this only means gmock/
# gmock_main compile even though the test executables below link gtest_main only.
FetchContent_Declare(
    googletest
    GIT_REPOSITORY ${_hven_gtest_url}
    GIT_TAG        ${_hven_gtest_tag}
    GIT_SHALLOW    TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF)   # keep gtest out of any future cmake --install of hven
FetchContent_MakeAvailable(googletest)

# Clang with MSVC headers: suppress char8_t->char32_t implicit conversion
# (gmock/gmock_main compile gtest-printers.h too, and are built even though
# only gtest_main is linked; clang 22 -- required by the 14.52 MSVC STL --
# makes the warning fire there)
# warning in gtest-printers.h. gtest compiles with -WX so this becomes an
# error without the suppression.
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
    foreach(_t gtest gtest_main gmock gmock_main)
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
# equivalent on Windows -- so the test executables genuinely cannot load without
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

# The reference dispatch for exact route pins (tests/support/route_reference.h;
# docs/testing.md "The reference build"; WX-R13). The pins were measured at ONE MKL dispatch --
# the derivation box's -- and a Linux x86-64 clang MKL build on any other CPU is a different
# build in fact (CI run 40985070468: the same commit and image went 0 -> 39 route failures on
# an AVX-512 runner). So the build decides it by probing the host it configures on:
# tests/support/mkl_dispatch_probe.cpp prints the CPU vendor / AVX2 / FMA / AVX-512F bits
# and mkl_cbwr_get_auto_branch(), and exits 0 only on the reference signature. Every test target
# below sees HVEN_TEST_REFERENCE_DISPATCH=0|1 (route_reference.h refuses to compile without
# it). A probe that cannot compile or run is off-reference. OFF forces the portable arms (the
# local proof that a non-matching host compiles and passes them). The sentinel
# RouteReference.TheReferenceBuildRunsAtMklsAutoDispatch re-checks the signature at run time,
# so a reference binary carried to another host fails one readable test.
string(CONCAT _hven_route_doc
    "Route-pin reference dispatch: AUTO probes the configuring host's MKL dispatch; "
    "OFF forces the off-reference (portable) arms")
set(HVEN_TEST_ROUTE_REFERENCE_DISPATCH "AUTO" CACHE STRING "${_hven_route_doc}")
set_property(CACHE HVEN_TEST_ROUTE_REFERENCE_DISPATCH PROPERTY STRINGS AUTO OFF)
set(_hven_reference_dispatch 0)
set(_hven_dispatch_signature "not probed")
if(HVEN_TEST_ROUTE_REFERENCE_DISPATCH STREQUAL "OFF")
    set(_hven_dispatch_signature "forced OFF by HVEN_TEST_ROUTE_REFERENCE_DISPATCH")
elseif(NOT HVEN_TEST_ROUTE_REFERENCE_DISPATCH STREQUAL "AUTO")
    message(FATAL_ERROR "HVEN_TEST_ROUTE_REFERENCE_DISPATCH must be AUTO or OFF, got "
        "'${HVEN_TEST_ROUTE_REFERENCE_DISPATCH}'")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$"
       AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND NOT APPLE AND MKL_FOUND
       AND NOT CMAKE_CROSSCOMPILING)
    unset(_hven_dispatch_run CACHE)
    unset(_hven_dispatch_compile CACHE)
    try_run(_hven_dispatch_run _hven_dispatch_compile
        "${CMAKE_CURRENT_BINARY_DIR}/mkl_dispatch_probe"
        "${CMAKE_CURRENT_SOURCE_DIR}/support/mkl_dispatch_probe.cpp"
        CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${INCLUDE_DIRS}"
        LINK_LIBRARIES ${LINK_LIBS}
        CXX_STANDARD 20
        COMPILE_OUTPUT_VARIABLE _hven_dispatch_compile_out
        RUN_OUTPUT_VARIABLE _hven_dispatch_run_out)
    string(STRIP "${_hven_dispatch_run_out}" _hven_dispatch_run_out)
    if(NOT _hven_dispatch_compile)
        set(_hven_dispatch_signature "probe did not compile")
        message(WARNING "hven tests: the MKL dispatch probe did not compile, so this build is "
            "off-reference:\n${_hven_dispatch_compile_out}")
    elseif(_hven_dispatch_run STREQUAL "0")
        set(_hven_reference_dispatch 1)
        set(_hven_dispatch_signature "${_hven_dispatch_run_out}")
    else()
        set(_hven_dispatch_signature "${_hven_dispatch_run_out} (exit ${_hven_dispatch_run})")
    endif()
else()
    set(_hven_dispatch_signature "not Linux x86-64 clang MKL")
endif()
if(_hven_reference_dispatch)
    message(STATUS
        "hven tests: MKL dispatch ${_hven_dispatch_signature} -> REFERENCE (route pins asserted)")
else()
    message(STATUS
        "hven tests: MKL dispatch ${_hven_dispatch_signature} -> OFF-REFERENCE (portable arms)")
endif()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/hven_mkl_dispatch.txt"
    "${_hven_dispatch_signature}\nHVEN_TEST_REFERENCE_DISPATCH=${_hven_reference_dispatch}\n")
add_compile_definitions(HVEN_TEST_REFERENCE_DISPATCH=${_hven_reference_dispatch})

# Release-only route pins (a route measured under Release's rounding and asserted only there) are
# keyed on the BUILD TYPE, not on NDEBUG: RelWithDebInfo and MinSizeRel define NDEBUG too, and
# their -O2 / -Os code is not the build the pin was measured on. HVEN_TEST_RELEASE_BUILD is 1
# exactly in a Release build (single- or multi-config), 0 otherwise; route_reference.h refuses to
# compile without it.
add_compile_definitions(HVEN_TEST_RELEASE_BUILD=$<IF:$<CONFIG:Release>,1,0>)

# PF-6: 1 when hven links mkl_sequential (HVEN_MKL_THREADING=sequential). Tests whose premise is that
# MKL honours a thread-count override skip themselves under it (support/mkl_threading.h).
if(HVEN_MKL_THREADING STREQUAL "sequential" AND NOT APPLE)
    add_compile_definitions(HVEN_TEST_MKL_SEQUENTIAL=1)
else()
    add_compile_definitions(HVEN_TEST_MKL_SEQUENTIAL=0)
endif()

# The include root of every test and bench target that spells a local include from the tests
# root ("qp/support/ssn_fixtures.h", "support/console_capture.h"). A target opts in with
# target_include_directories(<t> PRIVATE ${HVEN_TESTS_INCLUDE_ROOT}); the variable is visible in
# every tests/ subdirectory (bench/ spells the same directory as ../tests).
set(HVEN_TESTS_INCLUDE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

# Because fixtures are spelled from that root, a fixture's basename must be unique across all
# support/ directories: two same-named fixtures would let a quoted include resolve to the wrong
# one. The configure fails on a repeat (S7 of the restructure design).
file(GLOB_RECURSE _hven_support_files CONFIGURE_DEPENDS LIST_DIRECTORIES false
    "${CMAKE_CURRENT_SOURCE_DIR}/support/*"
    "${CMAKE_CURRENT_SOURCE_DIR}/*/support/*")
set(_hven_support_basenames "")
foreach(_f IN LISTS _hven_support_files)
    get_filename_component(_name "${_f}" NAME)
    if(_name IN_LIST _hven_support_basenames)
        message(FATAL_ERROR "tests: the fixture basename '${_name}' appears in more than one "
            "support/ directory (second one: ${_f}); fixtures are included from the tests "
            "root, so each basename must be unique across all support/ directories.")
    endif()
    list(APPEND _hven_support_basenames "${_name}")
endforeach()

################################################################################
# One ordinary test executable per tests/ directory
#
# tests/<dir>/CMakeLists.txt defines hven_<dir>_tests for core, linear, model, kkt, qp,
# warmstart, solver, trace, ipm, sqp and bench; the sources of an executable are the .cpp files
# of its directory (docs/testing.md, "Run, add, find"). Two kinds of process stay separate:
#   - SEAM executables (hven_<dir>_seam_tests; at most one per directory). They compile library
#     translation units with HVEN_TESTING defined and cannot share a binary with ordinary tests;
#     none links hven::hven. Their sources live in tests/<dir>/seam/ and nowhere else.
#   - hven_linear_alloc_tests, which replaces the global operator new and wraps malloc at link
#     time, so it needs its own process (hven_add_isolated_test_executable).
#
# COMPILE FLAGS, one rule per directory. Every test target takes ${COMPILE_FLAGS}, the same flag
# regime as the library (CLAUDE.md section 7). A directory whose fixtures are compile-heavy and
# whose tests do not depend on the optimisation level of the test code itself builds at -O1 in
# Release (the RELEASE_O1 argument of its hven_add_test_executable call): ipm, model, solver and
# trace. The others build at the project's level. There is no per-file override and no unity
# batching: a test file includes what it uses.
#
# The helpers below are the only way a test source reaches an executable, and they keep a
# registry, so the build itself answers "is every test source built exactly once, and in the
# right kind of executable?":
#   - hven_check_test_source_membership() (end of this file) fails the configure if a
#     tests/<dir>/*.cpp (or <dir>/support or <dir>/seam) is in no executable, or in two, or is
#     claimed but missing;
#   - a source a platform does not build is recorded with hven_test_sources_absent() (or
#     hven_sources_not_on_windows()) so the absence is explicit, not silent;
#   - a seam executable taking a source from outside tests/<dir>/seam/, or an ordinary one
#     taking a source from inside it, fails the configure;
#   - hven_check_seam_targets() fails the configure if a seam executable links hven::hven or an
#     ordinary executable defines HVEN_TESTING.
################################################################################

get_filename_component(HVEN_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(HVEN_TESTS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

# Rooted at THIS directory, not at ${CMAKE_SOURCE_DIR}. Tests are on by default, and
# CMAKE_SOURCE_DIR is the top-level source directory of whatever build is running -- which
# is hven's own root only when hven is the top-level project. Under add_subdirectory(hven)
# from a parent project it is the PARENT's root, and configuration fails outright looking
# for hven's sources beneath a tree that does not contain them. The relative form is
# correct in both cases and needs no knowledge of who is building.
set(HVEN_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src)

set_property(GLOBAL PROPERTY HVEN_TEST_SRC_CLAIMED "")
set_property(GLOBAL PROPERTY HVEN_SEAM_TARGETS "")
set_property(GLOBAL PROPERTY HVEN_WHOLE_SEAM_TARGETS "")
set_property(GLOBAL PROPERTY HVEN_NARROW_SEAM_TARGETS "")
set_property(GLOBAL PROPERTY HVEN_ORDINARY_TEST_TARGETS "")

# Records that <owner> builds each given source (paths relative to the calling directory;
# anything outside tests/ -- a library or bench source compiled into a test executable --
# is not a test source and is not registered). A second claim of one source is an error.
function(_hven_claim_test_sources owner)
    foreach(_s IN LISTS ARGN)
        get_filename_component(_abs "${_s}" REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        file(RELATIVE_PATH _rel "${HVEN_TESTS_ROOT}" "${_abs}")
        if(_rel MATCHES "^\\.\\.")
            continue()
        endif()
        get_property(_prev GLOBAL PROPERTY "HVEN_TEST_SRC_OWNER:${_rel}")
        if(_prev)
            message(FATAL_ERROR "tests/${_rel} is built by two executables (${_prev} and "
                "${owner}); every test source belongs to exactly one.")
        endif()
        set_property(GLOBAL PROPERTY "HVEN_TEST_SRC_OWNER:${_rel}" "${owner}")
        set_property(GLOBAL APPEND PROPERTY HVEN_TEST_SRC_CLAIMED "${_rel}")
    endforeach()
endfunction()

# Records that these sources are deliberately not built in this configuration, with the
# reason (a platform gate or an absent optional dependency).
function(hven_test_sources_absent reason)
    _hven_claim_test_sources("(absent: ${reason})" ${ARGN})
endfunction()

# Seam sources live in tests/<dir>/seam/ and nowhere else; ordinary executables never take one.
# <want_seam> is TRUE for a seam executable's sources, FALSE for an ordinary one's. Sources outside
# tests/ (a library or bench source compiled in) are not test sources and are not checked.
function(_hven_check_seam_placement owner want_seam)
    foreach(_s IN LISTS ARGN)
        get_filename_component(_abs "${_s}" REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        file(RELATIVE_PATH _rel "${HVEN_TESTS_ROOT}" "${_abs}")
        if(_rel MATCHES "^\\.\\.")
            continue()
        endif()
        if(_rel MATCHES "^[a-z_]+/seam/[^/]+$")
            set(_in_seam TRUE)
        else()
            set(_in_seam FALSE)
        endif()
        if(want_seam AND NOT _in_seam)
            message(FATAL_ERROR "${owner} is a seam executable (HVEN_TESTING) but takes the source "
                "tests/${_rel}, which is not under tests/<dir>/seam/. Seam sources live in "
                "tests/<dir>/seam/ so that a reader sees which tests need the HVEN_TESTING build.")
        endif()
        if(NOT want_seam AND _in_seam)
            message(FATAL_ERROR "${owner} is an ordinary executable but takes the source "
                "tests/${_rel}, which is under tests/<dir>/seam/. Sources in seam/ need the "
                "HVEN_TESTING build and belong to the directory's hven_<dir>_seam_tests.")
        endif()
    endforeach()
endfunction()

# hven_add_test_executable(<name>
#     SOURCES <src>...          every source of the executable, relative to the calling directory
#     [RELEASE_O1]              build the executable at -O1 in Release (the directory's rule, see
#                               the COMPILE FLAGS block above); no per-file override exists
#     [INCLUDE_BENCH]           the executable includes bench/ headers, spelled "bench/<h>.h" from
#                               the repository root
#     [EXTRA_SOURCES <src>...]  non-test sources compiled in (bench/ipm_corpus_leg.cpp)
#     [DEFINITIONS <def>...]    compile definitions (the ones only this executable's sources read)
#     [TEST_FILTER <filter>])   gtest_discover_tests TEST_FILTER (registration, not execution)
# Links hven::hven and gtest_main, so it must never carry HVEN_TESTING (see the seam helper).
function(hven_add_test_executable name)
    cmake_parse_arguments(PARSE_ARGV 1 A "RELEASE_O1;INCLUDE_BENCH" "TEST_FILTER"
        "SOURCES;EXTRA_SOURCES;DEFINITIONS")
    if(NOT A_SOURCES)
        return()   # every source of this directory is gated off here: no executable
    endif()
    _hven_check_seam_placement(${name} FALSE ${A_SOURCES})
    add_executable(${name} ${A_SOURCES} ${A_EXTRA_SOURCES})
    _hven_claim_test_sources(${name} ${A_SOURCES})
    set_property(GLOBAL APPEND PROPERTY HVEN_ORDINARY_TEST_TARGETS ${name})
    target_link_libraries(${name} PRIVATE hven::hven GTest::gtest_main)
    target_compile_options(${name} PRIVATE ${COMPILE_FLAGS})
    target_include_directories(${name} PRIVATE ${HVEN_TESTS_INCLUDE_ROOT})
    if(A_INCLUDE_BENCH)
        target_include_directories(${name} PRIVATE ${HVEN_REPO_ROOT})
    endif()
    if(A_DEFINITIONS)
        target_compile_definitions(${name} PRIVATE ${A_DEFINITIONS})
    endif()
    set(_o1_ok FALSE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(_o1_ok TRUE)
    endif()
    if(A_RELEASE_O1 AND _o1_ok)
        target_compile_options(${name} PRIVATE "$<$<CONFIG:Release>:-O1>")
    endif()
    if(A_TEST_FILTER)
        gtest_discover_tests(${name} TEST_FILTER "${A_TEST_FILTER}"
            DISCOVERY_MODE ${HVEN_GTEST_DISCOVERY_MODE})
    else()
        gtest_discover_tests(${name} DISCOVERY_MODE ${HVEN_GTEST_DISCOVERY_MODE})
    endif()
endfunction()

# hven_add_isolated_test_executable(<name>
#     SOURCES <src>...             the sources, relative to the calling directory
#     [DEFINITIONS <def>...] [LINK_OPTIONS <opt>...])
# An ordinary executable (links hven::hven, no HVEN_TESTING) that needs a process of its own because
# its test changes something process-global at link time -- hven_linear_alloc_tests replaces
# operator new and wraps malloc, which no other test can share a binary with.
function(hven_add_isolated_test_executable name)
    cmake_parse_arguments(PARSE_ARGV 1 A "" "" "SOURCES;DEFINITIONS;LINK_OPTIONS")
    _hven_check_seam_placement(${name} FALSE ${A_SOURCES})
    add_executable(${name} ${A_SOURCES})
    _hven_claim_test_sources(${name} ${A_SOURCES})
    set_property(GLOBAL APPEND PROPERTY HVEN_ORDINARY_TEST_TARGETS ${name})
    target_link_libraries(${name} PRIVATE hven::hven GTest::gtest_main)
    target_compile_options(${name} PRIVATE ${COMPILE_FLAGS})
    target_include_directories(${name} PRIVATE ${HVEN_TESTS_INCLUDE_ROOT})
    if(A_DEFINITIONS)
        target_compile_definitions(${name} PRIVATE ${A_DEFINITIONS})
    endif()
    if(A_LINK_OPTIONS)
        target_link_options(${name} PRIVATE ${A_LINK_OPTIONS})
    endif()
    gtest_discover_tests(${name} DISCOVERY_MODE ${HVEN_GTEST_DISCOVERY_MODE})
endfunction()

# The platform's backend session and adapter TUs a seam executable recompiles beside the
# dense arm's session (the dispatch the adapter does) and the analysis tally (the record the
# adapter makes).
function(_hven_seam_backend_sources out)
    set(_l ${HVEN_SRC_DIR}/linear/dense_session.cpp ${HVEN_SRC_DIR}/linear/analysis_tally.cpp)
    if(NOT APPLE)
        list(APPEND _l ${HVEN_SRC_DIR}/linear/pardiso_session.cpp
                       ${HVEN_SRC_DIR}/linear/symmetric_factor_mkl.cpp)
    else()
        list(APPEND _l ${HVEN_SRC_DIR}/linear/accelerate_session.cpp
                       ${HVEN_SRC_DIR}/linear/symmetric_factor_accelerate.cpp)
    endif()
    set(${out} ${_l} PARENT_SCOPE)
endfunction()

# The HVEN_TESTING build of the whole library, compiled ONCE (RS-6a, owner ruling Q9). Every
# whole-library seam executable links these objects instead of recompiling the library itself, so
# adding a seam executable costs its test sources and one link, not another library build. The
# objects are the hven target's own sources and definitions plus HVEN_TESTING=1; nothing here links
# hven::hven, so the production library stays untouched by HVEN_TESTING. Created on first use.
function(_hven_testing_library)
    if(TARGET hven_testing_library)
        return()
    endif()
    add_library(hven_testing_library OBJECT $<TARGET_PROPERTY:hven,SOURCES>)
    target_compile_definitions(hven_testing_library PRIVATE
        $<TARGET_PROPERTY:hven,COMPILE_DEFINITIONS> HVEN_TESTING=1)
    target_include_directories(hven_testing_library PRIVATE
        $<TARGET_PROPERTY:hven,INCLUDE_DIRECTORIES> ${HVEN_TESTS_INCLUDE_ROOT})
    target_link_libraries(hven_testing_library PRIVATE
        Eigen3::Eigen fmt::fmt-header-only Threads::Threads ${LINK_LIBS})
    if(APPLE)
        target_link_libraries(hven_testing_library PRIVATE AccelerateSparse::AccelerateSparse)
    endif()
    target_compile_options(hven_testing_library PRIVATE ${COMPILE_FLAGS})
endfunction()

# hven_add_seam_executable(<name>
#     SOURCES <src>...             the seam test sources (relative to the calling directory)
#     ( LIBRARY_SOURCES <src>...   the library TUs to recompile with HVEN_TESTING (absolute), plus
#                                  the platform backend TUs, appended here: a NARROW seam, for a
#                                  directory whose closure is a handful of TUs
#     | WHOLE_LIBRARY )            or the whole library, linked from the shared HVEN_TESTING object
#                                  library hven_testing_library (compiled once for all of them)
#     [DEFINITIONS <def>...])
# A standalone executable on the HVEN_TESTING convention (docs/testing.md): HVEN_TESTING is
# target-wide on the library TUs it carries, and it does NOT link hven::hven: that keeps the
# production library's objects and every ordinary executable untouched by HVEN_TESTING, and
# libhven.a would define the same symbols a second time.
function(hven_add_seam_executable name)
    cmake_parse_arguments(PARSE_ARGV 1 A "WHOLE_LIBRARY" ""
        "SOURCES;LIBRARY_SOURCES;DEFINITIONS")
    _hven_check_seam_placement(${name} TRUE ${A_SOURCES})
    if(A_WHOLE_LIBRARY)
        _hven_testing_library()
        add_executable(${name} $<TARGET_OBJECTS:hven_testing_library> ${A_SOURCES})
        target_compile_definitions(${name} PRIVATE
            $<TARGET_PROPERTY:hven,COMPILE_DEFINITIONS> HVEN_TESTING=1 ${A_DEFINITIONS})
        target_include_directories(${name} PRIVATE
            $<TARGET_PROPERTY:hven,INCLUDE_DIRECTORIES> ${HVEN_TESTS_INCLUDE_ROOT})
        set_property(GLOBAL APPEND PROPERTY HVEN_WHOLE_SEAM_TARGETS ${name})
    else()
        _hven_seam_backend_sources(_backend)
        add_executable(${name} ${A_LIBRARY_SOURCES} ${_backend} ${A_SOURCES})
        target_compile_definitions(${name} PRIVATE HVEN_TESTING=1 ${A_DEFINITIONS})
        target_include_directories(${name} PRIVATE
            ${HVEN_TESTS_ROOT}/../include ${INCLUDE_DIRS} ${HVEN_TESTS_INCLUDE_ROOT})
        list(LENGTH A_LIBRARY_SOURCES _n_lib)
        set_property(GLOBAL APPEND PROPERTY HVEN_NARROW_SEAM_TARGETS ${name})
        set_property(GLOBAL PROPERTY "HVEN_NARROW_SEAM_LIBCOUNT:${name}" ${_n_lib})
    endif()
    _hven_claim_test_sources(${name} ${A_SOURCES})
    set_property(GLOBAL APPEND PROPERTY HVEN_SEAM_TARGETS ${name})
    target_link_libraries(${name} PRIVATE
        Eigen3::Eigen fmt::fmt-header-only Threads::Threads GTest::gtest_main ${LINK_LIBS})
    if(APPLE)
        target_link_libraries(${name} PRIVATE AccelerateSparse::AccelerateSparse)
    endif()
    target_compile_options(${name} PRIVATE ${COMPILE_FLAGS})
    gtest_discover_tests(${name} DISCOVERY_MODE ${HVEN_GTEST_DISCOVERY_MODE})
endfunction()

# Run once, after every tests/ subdirectory has been added.
function(hven_check_test_source_membership)
    set(_globbed "")
    foreach(_d IN ITEMS core linear model kkt qp warmstart solver trace ipm sqp bench)
        file(GLOB _g RELATIVE "${HVEN_TESTS_ROOT}" CONFIGURE_DEPENDS
            "${HVEN_TESTS_ROOT}/${_d}/*.cpp" "${HVEN_TESTS_ROOT}/${_d}/support/*.cpp"
            "${HVEN_TESTS_ROOT}/${_d}/seam/*.cpp")
        list(APPEND _globbed ${_g})
    endforeach()
    get_property(_claimed GLOBAL PROPERTY HVEN_TEST_SRC_CLAIMED)
    foreach(_g IN LISTS _globbed)
        if(NOT _g IN_LIST _claimed)
            message(FATAL_ERROR "tests/${_g} is in no test executable: add it to its directory's "
                "source list (or record it with hven_test_sources_absent() if this platform does "
                "not build it).")
        endif()
    endforeach()
    foreach(_c IN LISTS _claimed)
        if(NOT _c IN_LIST _globbed)
            message(FATAL_ERROR "tests/${_c} is claimed by an executable but is not a "
                "tests/<dir>/*.cpp source of any component directory.")
        endif()
    endforeach()
    # The membership, written out so a gate can compare it with the declared one.
    list(SORT _claimed)
    set(_table "")
    foreach(_c IN LISTS _claimed)
        get_property(_o GLOBAL PROPERTY "HVEN_TEST_SRC_OWNER:${_c}")
        string(APPEND _table "${_c} ${_o}\n")
    endforeach()
    file(WRITE "${CMAKE_BINARY_DIR}/hven_test_membership.txt" "${_table}")
    list(LENGTH _globbed _n)
    message(STATUS
        "hven tests: ${_n} test sources, each in exactly one executable or recorded absent")
endfunction()

# Run once, after every tests/ subdirectory has been added.
function(hven_check_seam_targets)
    get_property(_seams GLOBAL PROPERTY HVEN_SEAM_TARGETS)
    foreach(_t IN LISTS _seams)
        get_target_property(_libs ${_t} LINK_LIBRARIES)
        if("hven::hven" IN_LIST _libs OR "hven" IN_LIST _libs)
            message(FATAL_ERROR "${_t} is a seam executable (HVEN_TESTING) and links hven::hven; "
                "it must not (docs/testing.md: libhven.a would define the same symbols a second "
                "time and the production library would no longer be untouched by HVEN_TESTING).")
        endif()
    endforeach()
    # A whole-library seam must take the library from the shared object library, never compile
    # a library TU of its own (RS-6a, Q9): its sources are the test sources and
    # $<TARGET_OBJECTS:hven_testing_library>, nothing under src/. A narrow seam must stay narrow:
    # fewer library TUs than half the library, or it is a whole-library seam that was not declared
    # as one.
    get_property(_whole GLOBAL PROPERTY HVEN_WHOLE_SEAM_TARGETS)
    get_filename_component(_src_abs "${HVEN_SRC_DIR}" ABSOLUTE)
    foreach(_t IN LISTS _whole)
        get_target_property(_srcs ${_t} SOURCES)
        if(NOT "$<TARGET_OBJECTS:hven_testing_library>" IN_LIST _srcs)
            message(FATAL_ERROR "${_t} is a whole-library seam executable but does not link the "
                "shared HVEN_TESTING objects (hven_testing_library); the library is compiled "
                "once for all whole-library seams.")
        endif()
        foreach(_s IN LISTS _srcs)
            get_filename_component(_s_abs "${_s}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
            if(_s_abs MATCHES "^${_src_abs}/")
                message(FATAL_ERROR "${_t} is a whole-library seam executable and compiles the "
                    "library TU ${_s} itself; it must take it from hven_testing_library (the "
                    "library is compiled once for all whole-library seams).")
            endif()
        endforeach()
    endforeach()
    get_property(_narrow GLOBAL PROPERTY HVEN_NARROW_SEAM_TARGETS)
    get_target_property(_hven_srcs hven SOURCES)
    list(LENGTH _hven_srcs _n_hven)
    math(EXPR _narrow_max "${_n_hven} / 2")
    foreach(_t IN LISTS _narrow)
        get_property(_n_lib GLOBAL PROPERTY "HVEN_NARROW_SEAM_LIBCOUNT:${_t}")
        get_target_property(_srcs ${_t} SOURCES)
        set(_n_src 0)
        foreach(_s IN LISTS _srcs)
            get_filename_component(_s_abs "${_s}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
            if(_s_abs MATCHES "^${_src_abs}/")
                math(EXPR _n_src "${_n_src} + 1")
            endif()
        endforeach()
        if(_n_src GREATER _narrow_max)
            message(FATAL_ERROR "${_t} is a narrow seam executable but recompiles ${_n_src} of "
                "the library's ${_n_hven} TUs; a seam that needs most of the library must be "
                "declared WHOLE_LIBRARY and use the shared hven_testing_library.")
        endif()
    endforeach()
    get_property(_ordinary GLOBAL PROPERTY HVEN_ORDINARY_TEST_TARGETS)
    foreach(_t IN LISTS _ordinary)
        get_target_property(_defs ${_t} COMPILE_DEFINITIONS)
        foreach(_d IN LISTS _defs)
            if(_d MATCHES "^HVEN_TESTING")
                message(FATAL_ERROR "${_t} links hven::hven and defines HVEN_TESTING; the "
                    "definition belongs only on a seam executable.")
            endif()
        endforeach()
    endforeach()
endfunction()


# Header self-containment (RS-5): every header under include/hven compiles ALONE. One generated
# translation unit per header, `#include <hven/...>` as its only line, in an OBJECT library that is
# EXCLUDE_FROM_ALL (an ordinary build never compiles it) and is built by the ctest test below. A
# header that only compiled because an earlier include had already pulled in what it needs fails
# here, by name. The TUs are real compiles under the library's own interface (include
# directories, definitions, the project's COMPILE_FLAGS), not a syntax-only approximation: the
# objects they produce are empty, so the build is incremental and the cost is the parse.
#
# EXCLUDED, by these two explicit lists and nothing else: the headers that belong to exactly one
# sparse backend. The Apple-only ones name Apple frameworks (accelerate_session.h, lapacke_shim.h)
# or the Accelerate thread control (accelerate_threads.h); the MKL-only one (pardiso_session.h)
# includes mkl_pardiso.h, which an Apple build does not have. Each list is excluded only where its
# backend is absent and is checked like every other header where it is present. Adding a header to
# either list needs a reason that says why it cannot be parsed alone on the other platforms; "it is
# not self-contained" is not one.
set(HVEN_HEADER_CHECK_APPLE_ONLY
    hven/detail/ipm/accelerate_threads.h
    hven/detail/linear/accelerate_session.h
    hven/detail/linear/lapacke_shim.h)
set(HVEN_HEADER_CHECK_MKL_ONLY
    hven/detail/linear/pardiso_session.h)
file(GLOB_RECURSE _hven_header_list CONFIGURE_DEPENDS
    RELATIVE "${HVEN_REPO_ROOT}/include" "${HVEN_REPO_ROOT}/include/hven/*.h")
list(SORT _hven_header_list)
if(APPLE)
    list(REMOVE_ITEM _hven_header_list ${HVEN_HEADER_CHECK_MKL_ONLY})
else()
    list(REMOVE_ITEM _hven_header_list ${HVEN_HEADER_CHECK_APPLE_ONLY})
endif()
set(_hven_header_check_sources "")
foreach(_h IN LISTS _hven_header_list)
    string(REPLACE "/" "_" _stem "${_h}")
    set(_tu "${CMAKE_CURRENT_BINARY_DIR}/header_selfcheck/${_stem}.cpp")
    file(CONFIGURE OUTPUT "${_tu}" CONTENT "#include <${_h}>\n" @ONLY)
    list(APPEND _hven_header_check_sources "${_tu}")
endforeach()
add_library(hven_header_selfcheck OBJECT EXCLUDE_FROM_ALL ${_hven_header_check_sources})
target_link_libraries(hven_header_selfcheck PRIVATE hven::hven)
target_compile_options(hven_header_selfcheck PRIVATE ${COMPILE_FLAGS})
add_test(NAME HeaderSelfContained
    COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}" --config $<CONFIG>
            --target hven_header_selfcheck)
# It drives a nested `cmake --build` of the same build directory, like the compile_fail probes
# (tests/model/CMakeLists.txt), so it takes the same lock and never races them under ctest -j.
set_tests_properties(HeaderSelfContained PROPERTIES TIMEOUT 1800
    RESOURCE_LOCK hven_compile_fail_nested_build)

################################################################################
# The golden-numerics rig: the harness, the seam adapters, the traces, the
# expected-table machinery, and the consumed-surface audit tooling. Its native
# arms need nothing beyond this repository and run here on every build; its two
# old-seam arms are built only when their sibling checkout is named. See
# tests/golden_rig/CMakeLists.txt and docs/testing.md's rig section.
################################################################################

add_subdirectory(golden_rig)

################################################################################
# The per-component executables, one directory each (see the block comment above). The
# order is the dependency order the old file used; sqp and bench are added on every
# platform. The one source set a platform does not build is bench's corpus-runner-driving
# tests (POSIX fork/exec; argued in tests/bench/CMakeLists.txt).
################################################################################

add_subdirectory(core)
add_subdirectory(linear)
add_subdirectory(model)
add_subdirectory(kkt)
add_subdirectory(qp)
add_subdirectory(warmstart)
add_subdirectory(solver)
add_subdirectory(trace)
add_subdirectory(ipm)
add_subdirectory(sqp)
add_subdirectory(bench)

hven_check_test_source_membership()
hven_check_seam_targets()
