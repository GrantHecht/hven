################################################################################
# hven unit tests — Google Test via FetchContent (same pattern as tycho's
# tests/cpp/CMakeLists.txt and psiopt/tests/CMakeLists.txt).
################################################################################

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.17.0
    GIT_SHALLOW    TRUE
    EXCLUDE_FROM_ALL
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
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
)

add_executable(hven_tests ${HVEN_TEST_SOURCES})
target_link_libraries(hven_tests PRIVATE hven::hven GTest::gtest_main)
target_compile_options(hven_tests PRIVATE ${COMPILE_FLAGS})

gtest_discover_tests(hven_tests)
