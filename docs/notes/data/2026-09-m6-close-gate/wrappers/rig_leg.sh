#!/usr/bin/env bash
# THE GOLDEN RIG, THREE SEAMS: the rig-scoped ctest run, plus both inventories.
#
# THE ONE PLACE THIS GATE'S WRAPPERS DO NOT OR-FOLD A STATUS, AND WHY: the rig's
# EXPECTED result is NOT all-green (docs/testing.md:1391-1404) -- exactly one
# designed failure, with all three FailByDesignControl.* tests green.  Folding
# ctest's status would make the expected result a leg failure.  So ctest's
# status is CAPTURED, PRINTED and left to the report to adjudicate, and the
# wrapper instead guards the thing that folding would otherwise catch: that
# ctest RAN AT ALL.  A run whose log carries no "tests passed" summary line is a
# wrapper failure (exit 4), not a rig result.  Every OTHER status here is folded.
set -uo pipefail
source /home/ghecht/Projects/hven/.scratch/m6-close/env.sh
BLD=${HVEN_CLOSE_RIG_BLD:-$F/build-3seam}
CTEST=${HVEN_CLOSE_CTEST:-ctest}
worst=0
echo "=== full ctest inventory (build-3seam) ==="
"$CTEST" --test-dir "$BLD" -N > "$F/logs/rig-inventory-full.out" 2>&1
rc=$?; worst=$((worst | rc)); echo "INVENTORY_RC=$rc"
tail -1 "$F/logs/rig-inventory-full.out"
echo "=== rig-scoped inventory (Arms/*, the rig's own tests, the controls) ==="
"$CTEST" --test-dir "$BLD" -N -R 'Arms/|GoldenRig|FailByDesignControl' > "$F/logs/rig-inventory-scoped.out" 2>&1
rc=$?; worst=$((worst | rc)); echo "SCOPED_INVENTORY_RC=$rc  (worst $worst)"
tail -1 "$F/logs/rig-inventory-scoped.out"
echo "=== rig-scoped ctest run ==="
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 "$CTEST" --test-dir "$BLD" -j4 \
    -R 'Arms/|GoldenRig|FailByDesignControl' --output-on-failure \
    > "$F/logs/rig-ctest-scoped.out" 2>&1
rig_rc=$?
echo "RIG_SCOPED_CTEST_RC=$rig_rc  (captured, NOT folded: one designed failure is expected)"
if ! grep -qE 'tests passed,' "$F/logs/rig-ctest-scoped.out"; then
    echo "WRAPPER FAILURE: no ctest summary line -- the run did not happen; this is not a rig result."
    worst=$((worst | 4))
fi
tail -12 "$F/logs/rig-ctest-scoped.out"
echo "--- the failing tests, in full ---"
grep -E '^[[:space:]]*[0-9]+ - .*\((Failed|Subprocess aborted|Timeout|Exception)\)' "$F/logs/rig-ctest-scoped.out" || echo "(none)"
echo "--- FailByDesignControl cells ---"
grep -E 'FailByDesignControl' "$F/logs/rig-ctest-scoped.out" || echo "(none matched)"
echo "RIG_SCOPED_CTEST_RC=$rig_rc"
echo "RIG_LEG_WORST_RC=$worst"
exit $worst
