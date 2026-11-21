#!/usr/bin/env bash
# usage: build.sh <Release|Debug> <dir under build-pfe/>   (FRESH: refuses an existing dir)
# Run as: flock /tmp/box-build.lock bash build.sh ...   Flags = ../../legs/build.sh's (arm B's).
source "$(dirname "$0")/common.sh"
TYPE=${1:?}; DIR=$BP/${2:?}
[ -e "$DIR" ] && { echo "NOT_FRESH $DIR"; fin 1; }
cmake -S "$WT" -B "$DIR" -G Ninja -DCMAKE_BUILD_TYPE="$TYPE" -DHVEN_FP_MODE=SAFER_FAST \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE \
  -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_EXAMPLES=ON > "$DIR.configure.log" 2>&1 || { echo CONFIGURE_FAIL; fin 1; }
ninja -C "$DIR" -j16 -k 0 2>&1 | tail -3
fin $?
