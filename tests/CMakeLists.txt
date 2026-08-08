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
)

add_executable(hven_tests ${HVEN_TEST_SOURCES})
target_link_libraries(hven_tests PRIVATE hven::hven GTest::gtest_main)
target_compile_options(hven_tests PRIVATE ${COMPILE_FLAGS})

# Same single source of truth as the hven target itself (root CMakeLists.txt)
# — see src/core/version.cpp and core/test_version.cpp.
target_compile_definitions(hven_tests PRIVATE HVEN_VERSION_STRING="${PROJECT_VERSION}")

gtest_discover_tests(hven_tests)
