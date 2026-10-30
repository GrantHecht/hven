#!/usr/bin/env bash
# usage: build.sh TAG -- (configure,) build + install the scratch Release lib into the bench copy's pin prefix,
# then (configure and) build the harness copy's hven_bench_cell. One locked leg.
set -o pipefail
R=/home/ghecht/Projects/hven/.scratch/m7w2r1
P=$R/bench/.hven/install-c1f730653d0d
tag="$1"
$R/tools/leg.sh "build-$tag" bash -c "
set -o pipefail
if [ ! -d $R/build-rel ]; then
  cmake -S $R/tree -B $R/build-rel -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -DHVEN_BUILD_TESTS=OFF -DHVEN_BUILD_BENCH=OFF -DHVEN_LINK_TIME_OPT=OFF -DCMAKE_INSTALL_PREFIX=$P > $R/logs/cfg-rel.log 2>&1 || exit 80
fi
cmake --build $R/build-rel --parallel 16 > $R/logs/build-rel-$tag.log 2>&1 || { tail -30 $R/logs/build-rel-$tag.log; exit 81; }
cmake --install $R/build-rel > $R/logs/install-$tag.log 2>&1 || exit 82
{ echo 'HVEN_PIN_SHA c1f730653d0db17c07f356b1d8136e862679f655'; echo 'HVEN_PIN_BRANCH m7'; echo 'HVEN_PIN_DATE 2026-09-25';
  echo \"HVEN_BUILT_UTC \$(date -u +%Y-%m-%dT%H:%M:%SZ)\"; echo \"HVEN_CXX_COMPILER \$(clang++ --version | head -1)\";
  echo \"HVEN_EIGEN_ALIGN \$(grep -m1 'EIGEN_MAX_ALIGN_BYTES resolved to' $R/logs/cfg-rel.log | sed 's/.*hven: //')\";
  echo 'HVEN_CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -DHVEN_BUILD_TESTS=OFF -DHVEN_BUILD_BENCH=OFF -DHVEN_LINK_TIME_OPT=OFF';
  echo 'HVEN_SOURCE_WORKTREE $R/tree (SCRATCH: git archive c1f73065 + M7 W2 R1 env-gated instrumentation, NOT the pin)'; } > $P/HVEN_PIN.txt
echo INSTALL_OK
if [ ! -d $R/bench/build ]; then
  (cd $R/bench && ./configure.sh > $R/logs/cfg-bench.log 2>&1) || exit 83
fi
cmake --build $R/bench/build --target hven_bench_cell --parallel 16 > $R/logs/build-bench-$tag.log 2>&1 || { tail -30 $R/logs/build-bench-$tag.log; exit 84; }
echo BENCH_OK
if [ ! -d $R/replay-build ]; then
  cmake -S $R/replay -B $R/replay-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_FLAGS=-DEIGEN_MAX_ALIGN_BYTES=32 -DCMAKE_PREFIX_PATH=$P > $R/logs/cfg-replay.log 2>&1 || exit 85
fi
cmake --build $R/replay-build > $R/logs/build-replay-$tag.log 2>&1 || { tail -30 $R/logs/build-replay-$tag.log; exit 86; }
echo REPLAY_OK
" 2>&1 | tee -a $R/logs/legs.log
