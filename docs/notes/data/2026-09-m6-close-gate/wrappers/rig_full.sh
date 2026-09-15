#!/usr/bin/env bash
# The WHOLE build-3seam ctest inventory run (the second number the brief asks
# for).  Same discipline as rig_leg.sh: ctest's status is captured and printed,
# never folded (one designed failure is the expected result); the wrapper folds
# only "did ctest run at all".
set -uo pipefail
source /home/ghecht/Projects/hven/.scratch/m6-close/env.sh
BLD=${HVEN_CLOSE_RIG_BLD:-$F/build-3seam}
CTEST=${HVEN_CLOSE_CTEST:-ctest}
worst=0
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 "$CTEST" --test-dir "$BLD" -j4 --output-on-failure \
    > "$F/logs/rig-ctest-full.out" 2>&1
rc=$?
echo "RIG_FULL_CTEST_RC=$rc  (captured, NOT folded)"
if ! grep -qE 'tests passed,' "$F/logs/rig-ctest-full.out"; then
    echo "WRAPPER FAILURE: no ctest summary line -- the run did not happen."
    worst=$((worst | 4))
fi
tail -12 "$F/logs/rig-ctest-full.out"
echo "--- the failing tests, in full ---"
grep -E '^[[:space:]]*[0-9]+ - .*\((Failed|Subprocess aborted|Timeout|Exception)\)' "$F/logs/rig-ctest-full.out" || echo "(none)"
echo "RIG_FULL_CTEST_RC=$rc"
echo "RIG_FULL_WORST_RC=$worst"
exit $worst
