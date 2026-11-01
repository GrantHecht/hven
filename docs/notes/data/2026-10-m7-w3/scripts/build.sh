#!/bin/bash
# usage: build.sh SRC BUILDDIR TYPE [targets...]   (configure line = the toolchain re-baseline's)
set -o pipefail
SRC=$1; B=$2; T=$3; shift 3
cmake -S "$SRC" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=$T -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DHVEN_BUILD_TESTS=ON -DHVEN_BUILD_BENCH=ON || exit 91
if [ $# -gt 0 ]; then cmake --build "$B" -j "$(nproc)" --target "$@" || exit 92
else cmake --build "$B" -j "$(nproc)" || exit 92; fi
grep -E "^(CMAKE_CXX_COMPILER|MKL_INCLUDE_DIR)" "$B/CMakeCache.txt"
echo BUILD_OK
