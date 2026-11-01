#!/usr/bin/env bash
# M7 W2 M1 fix round 1 -- the include-removal compile check: rebuild the test
# binary in the clean-configured Release tree.
set -o pipefail
S=/home/ghecht/Projects/hven/.scratch/m7w2m1
cmake --build "$S/rel-clean" -j "$(nproc)" --target hven_sqp_tests > "$S/logs/compile-fix1.txt" 2>&1
rc=$?
grep -E "error:|warning:|FAILED" "$S/logs/compile-fix1.txt" | head -20
tail -2 "$S/logs/compile-fix1.txt"
echo "COMPILE_RC=$rc"
exit $rc
