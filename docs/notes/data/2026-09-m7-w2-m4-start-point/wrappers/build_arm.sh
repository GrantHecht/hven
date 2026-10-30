#!/usr/bin/env bash
# Build ONE measurement arm: an hven Release install whose
# SqpOptions::project_start_point default is $ARM, plus the harness's OWN cell
# binary (its source untouched) configured OUT OF TREE against that prefix.
#
# THE TWO ARMS DIFFER BY ONE LITERAL and by nothing else. The harness's cell
# binary has no way to set a field its pin (3904a477) does not know about, so
# the toggle is exercised through the SHIPPED DEFAULT the binary compiles
# against. The literal is restored before the script returns, and the arm's
# installed header is sha256'd so the artifact can say which arm a row came from.
set -o pipefail
set -e
ARM="$1"                      # off | on
[[ "$ARM" == "off" || "$ARM" == "on" ]] || { echo "arm must be off|on"; exit 2; }
case "$ARM" in off) WANT=false;; on) WANT=true;; esac

ROOT=/home/ghecht/Projects/hven
SCRATCH="$ROOT/.scratch/m7w2m4"
HDR="$ROOT/include/hven/drivers/sqp_solver_types.h"
PREFIX="$SCRATCH/install-$ARM"
BUILD="$SCRATCH/hven-$ARM"
BENCH="$SCRATCH/bench-$ARM"

restore() { sed -i 's/^    bool project_start_point = true;$/    bool project_start_point = false;/' "$HDR"; }
trap restore EXIT

sed -i "s/^    bool project_start_point = .*;\$/    bool project_start_point = ${WANT};/" "$HDR"
grep -n "bool project_start_point" "$HDR"

cmake -S "$ROOT" -B "$BUILD" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DHVEN_BUILD_TESTS=OFF -DHVEN_BUILD_BENCH=OFF
cmake --build "$BUILD" -j "$(nproc)"
cmake --install "$BUILD"
sha256sum "$PREFIX/include/hven/drivers/sqp_solver_types.h" | tee "$SCRATCH/logs/arm-$ARM-header.sha256"
grep -n "bool project_start_point" "$PREFIX/include/hven/drivers/sqp_solver_types.h"

# The harness, OUT OF TREE, its source untouched. CMAKE_PREFIX_PATH is the one
# pin variable its CMake lets a caller set (`if(NOT CMAKE_PREFIX_PATH)`);
# HVEN_PIN_SHA is set unconditionally by cmake/hven_pin.cmake and CANNOT be
# overridden, so HVEN_BENCH_PIN_SHA in these binaries reads 3904a477 and is
# FALSE for them. Nothing in this task quotes that stamp.
cmake -S /home/ghecht/Projects/hven-bench -B "$BENCH" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_PREFIX_PATH="$PREFIX" \
  -DCMAKE_CXX_FLAGS="-DEIGEN_MAX_ALIGN_BYTES=32"
cmake --build "$BENCH" --target hven_bench_cell -j "$(nproc)"
ls -l "$BENCH/lanes/hven/hven_bench_cell"
