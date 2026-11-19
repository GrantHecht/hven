################################################################################
# core/ tests: hven_core_tests. Links hven::hven and nothing engine-specific.
# test_layering reads source text rather than linking anything; it is given the two trees it
# scans as absolute paths (rooted at the repository, never at CMAKE_SOURCE_DIR, which under
# add_subdirectory(hven) is the PARENT's root), and asserts they exist and contain the files it
# expects, so a wrong path fails loudly instead of scanning nothing.
# Flags: the project's level (the directory rule is in tests/CMakeLists.txt).
################################################################################

set(_hven_core_sources
    test_inf_norm.cpp
    test_layering.cpp
    test_pattern_hash.cpp
    test_type_storage.cpp
    test_version.cpp
)
hven_sources_not_on_windows(_hven_core_sources
    test_ledger.cpp
)

# HVEN_VERSION_STRING is the same single source of truth as the hven target itself (root
# CMakeLists.txt); see src/core/version.cpp and test_version.cpp.
hven_add_test_executable(hven_core_tests
    SOURCES ${_hven_core_sources}
    DEFINITIONS
        HVEN_VERSION_STRING="${PROJECT_VERSION}"
        HVEN_INCLUDE_ROOT="${HVEN_REPO_ROOT}/include/hven"
        HVEN_SRC_ROOT="${HVEN_REPO_ROOT}/src")
