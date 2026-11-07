#!/usr/bin/env bash
# usage: offref.sh <evidence dir>: linux-clang-release preset, -DHVEN_TEST_ROUTE_REFERENCE_DISPATCH=OFF, bench +
# examples ON, CLEAN into build-pfe/offref-r (refuses an existing dir), then ctest -j4 under setsid.
source "$(dirname "$0")/common.sh"
EV=${1:?}; DIR=$BP/offref-r; mkdir -p "$EV"
[ -e "$DIR" ] && { echo "NOT_FRESH $DIR"; fin 1; }
cd "$WT" || fin 1
cmake --preset linux-clang-release -B "$DIR" -DHVEN_TEST_ROUTE_REFERENCE_DISPATCH=OFF \
  -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_EXAMPLES=ON > "$EV/configure.log" 2>&1 || { echo CONFIGURE_FAIL; fin 1; }
ninja -C "$DIR" -j16 > "$EV/build.log" 2>&1; rc=$?
echo "build rc=$rc warning-lines=$(grep -cE 'warning:' "$EV/build.log")"
[ $rc -ne 0 ] && fin $rc
cd "$DIR" || fin 1
setsid ctest -j4 --output-on-failure > "$EV/ctest-release.log" 2>&1; rc=$?
grep -E "tests passed|tests failed" "$EV/ctest-release.log"
fin $rc
