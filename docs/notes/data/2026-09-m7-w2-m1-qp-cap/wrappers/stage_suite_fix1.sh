#!/usr/bin/env bash
# M7 W2 M1 FIX ROUND 1 -- the Release suite at the fix commit, one lock
# acquisition: build the clean-configured Release tree fully, then run it.
set -o pipefail
R=/home/ghecht/Projects/hven
S=$R/.scratch/m7w2m1
echo "=== HEAD $(git -C "$R" rev-parse --short HEAD)  dirty-files=$(git -C "$R" status --porcelain | wc -l) ==="
cmake --build "$S/rel-clean" -j "$(nproc)" > "$S/logs/build-release-fix1.txt" 2>&1
rc=$?; grep -E "error:|FAILED" "$S/logs/build-release-fix1.txt" | head -20; tail -2 "$S/logs/build-release-fix1.txt"
[ $rc -eq 0 ] || exit 1
echo "=== RELEASE SUITE ==="
cd "$S/rel-clean" || exit 1
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 ctest -j4 --output-on-failure 2>&1 | tail -25
exit ${PIPESTATUS[0]}
