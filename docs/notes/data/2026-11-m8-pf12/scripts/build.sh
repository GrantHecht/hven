#!/usr/bin/env bash
# PF-12 build recipe. $1 = source tree, $2 = build dir. Uniform flag regime as
# CMakePresets.json linux-clang-release (binaryDir overridden), ccache disabled.
set -o pipefail
SRC=${1:?src}; B=${2:?build}
rm -rf "$B"; mkdir -p "$B"
CCACHE_DISABLE=1 cmake -S "$SRC" -B "$B" -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE \
  -DHVEN_FP_MODE=SAFER_FAST -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_BUILD_TYPE=Release \
  > "$B.cfg.log" 2>&1 || { echo CONFIGURE_FAIL; exit 1; }
CCACHE_DISABLE=1 cmake --build "$B" --target hven_sqp_corpus -j${BOX_LANE_JOBS:-8} > "$B.build.log" 2>&1
rc=$?; echo "BUILD_RC=$rc"; sha256sum "$B/bench/hven_sqp_corpus"; exit $rc
