# M8 DN-1 A legs' replay tool (../../DECLARATION-dn1legs.txt): a consumer of an installed hven (find_package).
#   cmake -S tools -B <build> -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
#         -DCMAKE_PREFIX_PATH=<hven install prefix>
# EIGEN_MAX_ALIGN_BYTES=32: an installed hven's consumers must match the library's Eigen alignment.
cmake_minimum_required(VERSION 3.24)
project(hven_dn1_replay LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(hven REQUIRED)
add_executable(dn1_replay dn1_replay.cpp)
target_compile_definitions(dn1_replay PRIVATE EIGEN_MAX_ALIGN_BYTES=32)
target_link_libraries(dn1_replay PRIVATE hven::hven)
