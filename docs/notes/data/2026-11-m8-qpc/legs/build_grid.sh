#!/usr/bin/env bash
# QPC leg B helper: build hven_qp_dump_grid against one arm's hven install.
#   usage: build_grid.sh <hven install prefix> <hven tree (for tests/qp/support/qp_dump.h)> <build dir>
# Prints the grid's sha256 (recorded in BINARIES.txt as "QPC grid <arm> <pin> <sha256>").
# Run under: ~/.local/bin/box-lane -- bash build_grid.sh ...
set -euo pipefail
prefix=$(realpath "${1:?prefix}"); tree=$(realpath "${2:?hven tree}"); bdir=$(realpath -m "${3:?build dir}")
here=$(cd "$(dirname "$0")" && pwd)
cmake -S "$here/grid" -B "$bdir" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="${HVEN_BENCH_CXX:-clang++}" \
      -DCMAKE_PREFIX_PATH="$prefix" -Uhven_DIR -DHVEN_TESTS_DIR="$tree/tests" > "$bdir.configure.log" 2>&1 \
  || { tail -30 "$bdir.configure.log"; exit 1; }
cmake --build "$bdir" > "$bdir.build.log" 2>&1 || { tail -30 "$bdir.build.log"; exit 1; }
sha256sum "$bdir/hven_qp_dump_grid"
