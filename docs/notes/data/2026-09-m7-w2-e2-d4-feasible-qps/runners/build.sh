#!/usr/bin/env bash
# usage: build.sh TAG -- build + install the scratch Release lib, then (configure and) build the harness copy and the replay tool
set -o pipefail
D=/home/ghecht/Projects/hven/.scratch/m7w2e2d4
tag="$1"
$D/tools/leg.sh "build-$tag" bash -c "
set -o pipefail
cmake --build $D/build-rel -j16 2>&1 | tail -5 || exit 81
cmake --install $D/build-rel > $D/logs/install-$tag.log 2>&1 || exit 82
echo INSTALL_OK
[ -d $D/bench-build ] || cmake -S $D/bench -B $D/bench-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_PREFIX_PATH=$D/install > $D/logs/cfg-bench.log 2>&1 || exit 83
cmake --build $D/bench-build --target hven_bench_cell 2>&1 | tail -3 || exit 84
[ -d $D/replay-build ] || cmake -S $D/tools/replay -B $D/replay-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_FLAGS=-DEIGEN_MAX_ALIGN_BYTES=32 -DCMAKE_PREFIX_PATH=$D/install > $D/logs/cfg-replay.log 2>&1 || exit 85
cmake --build $D/replay-build 2>&1 | tail -3 || exit 86
" >> $D/logs/legs.log 2>&1
rc=$?; tail -14 $D/logs/legs.log; exit $rc
