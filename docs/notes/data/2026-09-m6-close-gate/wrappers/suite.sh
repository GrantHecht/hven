#!/usr/bin/env bash
# ONE SUITE LEG: ctest, then the `ctest -N` inventory -- every status CAPTURED
# and OR-FOLDED into `worst`, the leg exiting with it, so the trailing inventory
# CANNOT mask a test failure.  The build is its own leg (build_one.sh).
#   usage: suite.sh <tag> <abs-build-dir> [extra ctest args]
set -o pipefail
source /home/ghecht/Projects/hven/.scratch/m6-close/env.sh
tag="$1"; BLD="${2:-$F/build-$1}"; shift 2 || shift 1
worst=0
echo "=== ctest ($tag) ==="
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 ctest --test-dir "$BLD" -j4 "$@" \
    > "$F/logs/suite-ctest-$tag.out" 2>&1
rc=$?; worst=$((worst | rc)); echo "CTEST_RC=$rc  (worst $worst)"
tail -6 "$F/logs/suite-ctest-$tag.out"
echo "=== ctest -N inventory ($tag) -- AFTER the run, and unable to mask it ==="
ctest --test-dir "$BLD" -N > "$F/logs/suite-inv-$tag.out" 2>&1
rc=$?; worst=$((worst | rc)); echo "INVENTORY_RC=$rc  (worst $worst)"
tail -1 "$F/logs/suite-inv-$tag.out"
echo "SUITE_WORST_RC=$worst"
exit $worst
