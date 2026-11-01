#!/usr/bin/env bash
# Build ONE measurement arm: an hven Release install whose two M1 levers carry
# the arm's values as their SHIPPED DEFAULTS, plus the harness's OWN cell binary
# (its source untouched) configured OUT OF TREE against that prefix.
#
# THE ARMS DIFFER BY TWO LITERALS AND BY NOTHING ELSE. The harness's cell binary
# at its pin (3904a477) has no way to set a field that pin does not know about,
# so a toggle is exercised through the SHIPPED DEFAULT the binary compiles
# against -- M4's own construction. Both literals are restored before the script
# returns, and the arm's installed header is sha256'd so the artifact can say
# which arm a row came from.
set -o pipefail
set -e
ARM="$1"                      # off | budget | retry | both
case "$ARM" in
  off)    WANT_BUDGET=1; WANT_RETRY=false;;
  budget) WANT_BUDGET=3; WANT_RETRY=false;;
  retry)  WANT_BUDGET=1; WANT_RETRY=true;;
  both)   WANT_BUDGET=3; WANT_RETRY=true;;
  *) echo "arm must be off|budget|retry|both"; exit 2;;
esac

ROOT=/home/ghecht/Projects/hven
SCRATCH="$ROOT/.scratch/m7w2m1"
HDR="$ROOT/include/hven/drivers/sqp_solver_types.h"
PREFIX="$SCRATCH/install-$ARM"
BUILD="$SCRATCH/hven-$ARM"
BENCH="$SCRATCH/bench-$ARM"
mkdir -p "$SCRATCH/logs"

restore() {
  sed -i 's/^    Index qp_failure_budget = .*;$/    Index qp_failure_budget = 1;/' "$HDR"
  sed -i 's/^    bool qp_cap_retry = .*;$/    bool qp_cap_retry = false;/' "$HDR"
}
trap restore EXIT

sed -i "s/^    Index qp_failure_budget = .*;\$/    Index qp_failure_budget = ${WANT_BUDGET};/" "$HDR"
sed -i "s/^    bool qp_cap_retry = .*;\$/    bool qp_cap_retry = ${WANT_RETRY};/" "$HDR"
grep -n "Index qp_failure_budget = \|bool qp_cap_retry = " "$HDR"

cmake -S "$ROOT" -B "$BUILD" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DHVEN_BUILD_TESTS=OFF -DHVEN_BUILD_BENCH=OFF
cmake --build "$BUILD" -j "$(nproc)"
cmake --install "$BUILD"
sha256sum "$PREFIX/include/hven/drivers/sqp_solver_types.h" | tee "$SCRATCH/logs/arm-$ARM-header.sha256"
grep -n "Index qp_failure_budget = \|bool qp_cap_retry = " "$PREFIX/include/hven/drivers/sqp_solver_types.h"

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
