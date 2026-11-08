# M8 F1: first-solve probe on small Hock-Schittkowski problems, built against an hven install prefix.
cmake_minimum_required(VERSION 3.24)
project(hven_f1_hs_probe LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(hven REQUIRED)
add_executable(hs_probe hs_probe.cpp)
target_link_libraries(hs_probe PRIVATE hven::hven)
