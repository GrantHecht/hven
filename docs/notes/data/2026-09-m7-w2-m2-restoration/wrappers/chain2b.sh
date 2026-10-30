#!/usr/bin/env bash
# M7 W2 M2 chain 2: an hven Release INSTALL at this task's own HEAD (the S5
# commit), the ladder probe built out of tree against it, and the probe's own
# smoke + callback A/B on one small cell.
#
# The harness is configured READ-ONLY (`add_subdirectory` from the probe's own
# CMakeLists); nothing in `hven-bench` is written. `HVEN_BENCH_PIN_SHA` in any
# binary built here reads the harness's own pin, which is FALSE for this build
# and is quoted nowhere.
set -o pipefail
ROOT=/home/ghecht/Projects/hven
S=$ROOT/.scratch/m7w2m2
LOG=$S/logs/chain2b.log
mkdir -p "$S/logs"
exec >"$LOG" 2>&1
echo "CHAIN2B_START $(date -Is)"
flock /tmp/box-build.lock bash -c '
set -o pipefail
ROOT=/home/ghecht/Projects/hven
S=$ROOT/.scratch/m7w2m2
echo "FOREGROUND_START chain2b $(date -Is)"
cd "$ROOT" || exit 90
echo "=== HEAD $(git rev-parse --short HEAD)  dirty-files=$(git status --porcelain | wc -l) ==="
cmake -S "$ROOT" -B "$S/rel" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_INSTALL_PREFIX="$S/install" \
  -DHVEN_BUILD_TESTS=OFF -DHVEN_BUILD_BENCH=OFF || exit 91
cmake --build "$S/rel" -j "$(nproc)" || exit 91
cmake --install "$S/rel" || exit 91
echo "--- LIBRARY SHA256 (Fedora installs to lib64)"
sha256sum "$S/install/lib64/libhven.a"
grep -n "restoration_h_out" "$S/install/include/hven/core/solver_counters.h" | head -3
# A FRESH probe configure every time: `hven_DIR` is a cache variable that
# `find_package` sets on first success and then reuses, which is the exact
# stale-prefix trap the harness just added a guard for.
rm -rf "$S/probebuild"
cmake -S "$S/probe" -B "$S/probebuild" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_PREFIX_PATH="$S/install" || exit 92
echo "--- THE hven PACKAGE THIS PROBE RESOLVED"
grep -m1 "^hven_DIR:" "$S/probebuild/CMakeCache.txt"
cmake --build "$S/probebuild" --target m2_probe -j "$(nproc)" || exit 92
echo "--- PROBE SHA256"
sha256sum "$S/probebuild/m2_probe"
echo "--- SMOKE: ZAMB2-8, callback on then off"
cd /home/ghecht/Projects/hven-bench || exit 93
for arm in on off; do
  echo "### SMOKE ARM callback=$arm $(date -Is)"
  MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 timeout 600 \
    "$S/probebuild/m2_probe" --problem ZAMB2-8 --callback "$arm" \
    > "$S/logs/smoke-zamb2-8-$arm.txt" 2>&1
  echo "    rc=$?"
  head -40 "$S/logs/smoke-zamb2-8-$arm.txt"
done
echo "FOREGROUND_END chain2b $(date -Is) rc=0"
'
rc=$?
echo "WRAPPER_RC chain2b rc=${rc}"
echo "CHAIN2B_END $(date -Is)"
exit $rc
