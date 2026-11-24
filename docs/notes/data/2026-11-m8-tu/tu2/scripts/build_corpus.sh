#!/bin/bash
# build_corpus.sh SRC DIR TARGET : Release corpus build per RF-1 legs/build.sh recipe (SAFER_FAST, BENCH on)
set -o pipefail
SRC=$1; DIR=$2; TGT=${3:-hven_sqp_corpus}
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1
cmake -S "$SRC" -B "$DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release -DHVEN_FP_MODE=SAFER_FAST \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_EXAMPLES=ON > "$DIR.configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -20 "$DIR.configure.log"; exit 1; }
ninja -C "$DIR" -j${BOX_LANE_JOBS:-4} $TGT > "$DIR.build.log" 2>&1; rc=$?; tail -3 "$DIR.build.log"; echo rc=$rc; exit $rc
