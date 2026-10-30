#!/usr/bin/env bash
# RELEASE: a CLEAN configure (the bench binaries carry a configure-time
# git-describe, so every stamped leg below is built from empty at this HEAD),
# build, and the full Release suite.
#
# THE BUILD'S WHOLE OUTPUT GOES TO A FILE, stderr merged, and only the tail is
# echoed on success. On failure the compiler diagnostics are printed in full --
# chain1 round 1 lost them to a `| tail -15` that showed ninja's last fifteen
# PROGRESS lines and not the error above them.
set -o pipefail
R=/home/ghecht/Projects/hven
S=$R/.scratch/m7w2d1
B=$S/logs/release-build.log
echo "=== HEAD $(git -C "$R" rev-parse --short HEAD)  dirty-files=$(git -C "$R" status --porcelain | wc -l) ==="
rm -rf "$S/rel"
cmake -S "$R" -B "$S/rel" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DHVEN_BUILD_TESTS=ON -DHVEN_BUILD_BENCH=ON > "$S/logs/release-configure.log" 2>&1
rc=$?
tail -3 "$S/logs/release-configure.log"
if [ $rc -ne 0 ]; then echo "CONFIGURE FAILED rc=$rc"; cat "$S/logs/release-configure.log"; exit $rc; fi
cmake --build "$S/rel" -j "$(nproc)" > "$B" 2>&1
rc=$?
echo "BUILD_RC=$rc"
if [ $rc -ne 0 ]; then
  echo "--- BUILD FAILED: every diagnostic line, verbatim ---"
  grep -nE 'error:|warning:|FAILED:|ninja: ' "$B"
  echo "--- last 40 lines of the build log ---"
  tail -40 "$B"
  exit $rc
fi
echo "--- warnings, if any ---"
grep -cE 'warning:' "$B"
grep -E 'warning:' "$B" | head -20
cd "$S/rel" || exit 1
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 ctest -j4 --output-on-failure 2>&1 | tail -60
exit ${PIPESTATUS[0]}
