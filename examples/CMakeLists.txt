################################################################################
# hven examples: one self-contained program per problem.
#
# Two ways to build them:
#
#   * In tree:       cmake -S . -B build -DHVEN_BUILD_EXAMPLES=ON
#                    (the root CMakeLists.txt adds this directory; hven::hven is
#                    the in-tree alias target)
#   * Install tree:  configure THIS directory as its own top-level project,
#                    pointed at an hven install prefix, the way a stranger would:
#                    cmake -S examples -B ex-build -DCMAKE_PREFIX_PATH=<prefix>
#
# Each example returns non-zero when its solve fails, so `ctest` (or a plain
# run) is the check. Wiring them into CI is a separate task.
################################################################################

cmake_minimum_required(VERSION 3.24)

if(NOT TARGET hven::hven)
    project(hven_examples LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
    endif()
    find_package(hven REQUIRED)
    enable_testing()
endif()

set(HVEN_EXAMPLE_SOURCES
    01_hs071_both_engines
    02_bounded_rosenbrock
    03_equality_constrained
    04_partitioned_chain
    05_warm_continuation
    06_ipm_to_sqp_polish
    07_sqp_quasi_newton
    08_results_trace_options)

foreach(example IN LISTS HVEN_EXAMPLE_SOURCES)
    add_executable(hven_example_${example} ${example}.cpp)
    target_link_libraries(hven_example_${example} PRIVATE hven::hven)
    set_target_properties(hven_example_${example} PROPERTIES
        CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
    add_test(NAME hven_example_${example} COMMAND hven_example_${example})
endforeach()
