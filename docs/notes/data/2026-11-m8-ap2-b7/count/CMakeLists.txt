# M8 AP-2 B7: the TRAIN count (count_absent.cpp), configured as its own top-level project
# against an INSTALLED hven (find_package), with the source tree's bench/ and tests/sqp/
# on the include path for bench/corpus_cells.h and its test-support headers.
cmake_minimum_required(VERSION 3.24)
project(hven_b7_count_absent LANGUAGES CXX)
find_package(hven REQUIRED)
if(NOT DEFINED HVEN_SOURCE_ROOT)
    message(FATAL_ERROR "set -DHVEN_SOURCE_ROOT=<the hven checkout>")
endif()
add_executable(count_absent count_absent.cpp)
target_compile_features(count_absent PRIVATE cxx_std_20)
# Consumers of an installed hven pass the alignment the library was built with.
target_compile_definitions(count_absent PRIVATE EIGEN_MAX_ALIGN_BYTES=32)
target_include_directories(count_absent PRIVATE "${HVEN_SOURCE_ROOT}/bench"
                                                "${HVEN_SOURCE_ROOT}/tests/sqp")
target_link_libraries(count_absent PRIVATE hven::hven)
