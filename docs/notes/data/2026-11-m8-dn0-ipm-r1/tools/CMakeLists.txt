# M8 DN-0 replay tool: a consumer of an installed hven (find_package), never add_subdirectory.
#   cmake -S tools -B <build> -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
#         -DCMAKE_PREFIX_PATH=<hven install prefix>
cmake_minimum_required(VERSION 3.24)
project(hven_dn0_replay LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(hven REQUIRED)

add_executable(dn0_replay dn0_replay.cpp)
target_link_libraries(dn0_replay PRIVATE hven::hven)

add_executable(dn0_replay_verify dn0_replay.cpp)
target_compile_definitions(dn0_replay_verify PRIVATE DN0_VERIFY)
target_link_libraries(dn0_replay_verify PRIVATE hven::hven)
target_link_options(dn0_replay_verify PRIVATE "LINKER:--wrap=pardiso")

# The dump shim, built as an object for the harness's dump build (linked there with
# -Wl,--wrap=pardiso through CMAKE_EXE_LINKER_FLAGS; see tools/build_instruments.sh).
add_library(dn0_pardiso_wrap OBJECT pardiso_wrap.c)
set_target_properties(dn0_pardiso_wrap PROPERTIES POSITION_INDEPENDENT_CODE OFF)
