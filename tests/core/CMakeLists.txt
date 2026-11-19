################################################################################
# core/ tests: the executable hven_core_tests (RS-6a), the sources of this directory.
# It links hven::hven and nothing engine-specific. core/test_layering.cpp reads source text rather than linking anything, so it belongs to no backend and is not platform-gated;
# it is given the two trees it scans. Rooted at the repository, not at CMAKE_SOURCE_DIR (under add_subdirectory(hven) from a parent project that is the PARENT's root).
# Passed as absolute paths so the test is independent of the working directory ctest launches it from; the test asserts the directories exist and contain the files it expects,
# so a wrong path fails loudly instead of scanning nothing and passing.
################################################################################

set(_hven_core_sources
    test_inf_norm.cpp
    test_layering.cpp
    test_pattern_hash.cpp
    test_sparse_backend_link.cpp
    test_type_storage.cpp
    test_version.cpp
)

# The former hven_sqp_tests sources: UNIX-only (its subprocess wall-deadline tests drive the
# corpus runner, whose deadline enforcement is fork/exec orchestration, and the suite has never
# been built or derived on Windows; RS-6b is the stage that lifts the gate).
set(_hven_core_unix_sources
    test_ledger.cpp
)
if(NOT WIN32)
    list(APPEND _hven_core_sources ${_hven_core_unix_sources})
else()
    hven_test_sources_absent("Windows" ${_hven_core_unix_sources})
endif()

# Compiled at -O1 in Release, as the retired interior/drivers/model targets compiled them
# (compile time; the sources of the former hven_tests and hven_sqp_tests were not).
set(_hven_core_o1_sources
    test_type_storage.cpp
)

# test_type_storage was unity-batched in the retired interior target; alone in this executable it is the
# plain translation unit (a unity batch of one), so the executable is built non-unity.
# HVEN_VERSION_STRING is the same single source of truth as the hven target itself (root
# CMakeLists.txt) -- see src/core/version.cpp and core/test_version.cpp.
hven_add_test_executable(hven_core_tests
    SOURCES ${_hven_core_sources}
    O1_SOURCES ${_hven_core_o1_sources}
    DEFINITIONS
        HVEN_VERSION_STRING="${PROJECT_VERSION}"
        HVEN_INCLUDE_ROOT="${HVEN_REPO_ROOT}/include/hven"
        HVEN_SRC_ROOT="${HVEN_REPO_ROOT}/src")
