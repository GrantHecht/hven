# M8 AP-2 B3-a: the iparm dumper, configured as its own top-level project against an
# INSTALLED hven (find_package), once per arm. -DHVEN_B3_ARM=0 (pin) or 1 (B3).
cmake_minimum_required(VERSION 3.24)
project(hven_b3_iparm_dump LANGUAGES CXX)
find_package(hven REQUIRED)
if(NOT DEFINED HVEN_B3_ARM)
    message(FATAL_ERROR "set -DHVEN_B3_ARM=0 (pin) or 1 (B3)")
endif()
add_executable(dump_iparm dump_iparm.cpp)
target_compile_features(dump_iparm PRIVATE cxx_std_20)
target_compile_definitions(dump_iparm PRIVATE HVEN_B3_ARM=${HVEN_B3_ARM})
target_link_libraries(dump_iparm PRIVATE hven::hven)
target_link_options(dump_iparm PRIVATE "-Wl,--wrap=pardiso")
