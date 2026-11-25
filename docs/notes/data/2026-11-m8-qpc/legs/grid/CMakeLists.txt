# QPC SENS-Q grid (DECLARATION-QPC section 3): hven_qp_dump_grid built against one arm's hven INSTALL (find_package),
# the same source for every arm. HVEN_TESTS_DIR = an hven tree's tests/ (for qp/support/qp_dump.h, unchanged by QPC).
cmake_minimum_required(VERSION 3.24)
project(hven_qpc_grid LANGUAGES CXX)
find_package(hven REQUIRED)
if(NOT HVEN_TESTS_DIR)
    message(FATAL_ERROR "pass -DHVEN_TESTS_DIR=<hven tree>/tests")
endif()
add_executable(hven_qp_dump_grid qp_dump_grid.cpp)
target_compile_features(hven_qp_dump_grid PRIVATE cxx_std_20)
target_include_directories(hven_qp_dump_grid PRIVATE "${HVEN_TESTS_DIR}")
target_link_libraries(hven_qp_dump_grid PRIVATE hven::hven)
