#!/usr/bin/env bash
# DEBUG BUILD + DEBUG SUITE, excluding the one >600 s test, which runs ALONE in
# its own leg (the box protocol). Same full-log discipline as the Release leg.
set -o pipefail
R=/home/ghecht/Projects/hven
S=$R/.scratch/m7w2d1
B=$S/logs/debug-build.log
rm -rf "$S/dbg"
cmake -S "$R" -B "$S/dbg" -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DHVEN_BUILD_TESTS=ON -DHVEN_BUILD_BENCH=ON > "$S/logs/debug-configure.log" 2>&1
rc=$?
tail -3 "$S/logs/debug-configure.log"
if [ $rc -ne 0 ]; then echo "CONFIGURE FAILED rc=$rc"; cat "$S/logs/debug-configure.log"; exit $rc; fi
cmake --build "$S/dbg" -j "$(nproc)" > "$B" 2>&1
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
cd "$S/dbg" || exit 1
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 ctest -j4 --output-on-failure \
  -E '^Continuation\.ProbeBudgetBoundsAFailingProposal$' 2>&1 | tail -60
exit ${PIPESTATUS[0]}
