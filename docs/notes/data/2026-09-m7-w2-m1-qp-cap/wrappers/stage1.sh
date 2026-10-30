#!/usr/bin/env bash
# STAGE 1, one lock acquisition: the Release build and the whole Release suite.
set -o pipefail
R=/home/ghecht/Projects/hven
S=$R/.scratch/m7w2m1
cmake -S "$R" -B "$S/rel" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_INSTALL_PREFIX="$S/install" \
  -DHVEN_BUILD_TESTS=ON -DHVEN_BUILD_BENCH=ON 2>&1 | tail -5 || exit 1
cmake --build "$S/rel" -j "$(nproc)" > "$S/logs/build-release.txt" 2>&1; rc=$?; grep -E "error:|FAILED" "$S/logs/build-release.txt" | head -40; tail -3 "$S/logs/build-release.txt"; [ $rc -eq 0 ] || exit 1
echo "=== RELEASE SUITE ==="
cd "$S/rel"
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 ctest -j4 --output-on-failure 2>&1 | tail -80
exit ${PIPESTATUS[0]}
