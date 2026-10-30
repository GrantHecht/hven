#!/usr/bin/env bash
# SUITE v1 END-TO-END (brief section 3, criterion 1): hven-bench, ALL FOUR LANES (hven-ipm, hven-sqp, ipopt, snopt),
# the TRAIN roster of both pinned sets, ONE serial run per set (a run writes one budget_wall_s into every row, and the two
# sets are pinned under different ceilings: hs 600 s, cutest 300 s -- docs/runbook.md step 4), the options label `default`,
# iteration budget the lane's own (0 = the lane/engine default, as the runner's default and as H-REPIN ran), at the harness's
# committed pin.  HOLDOUT is never run (the runner refuses --split HOLDOUT; this script never passes --split).
# SNOPT / IPOPT are invoked only through the harness's committed lanes; no solver source is read.
# usage: suitev1.sh <step>   (step: pin | build | guards | hs | cutest) -- each run by run_leg.sh inside the box lock
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HERE/env.sh"
STEP="$1"; OUT="$F/suitev1"; mkdir -p "$OUT" "$LOGS"
cd "$BENCH" || exit 90
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
case "$STEP" in
 pin)    # build + install hven at the harness pin (clone of the pin commit, never a worktree; build_hven_pin.sh refuses a dirty/unpinned tree)
   ./scripts/build_hven_pin.sh; exit $?;;
 build)
   worst=0
   ./configure.sh; rc=$?; echo "CONFIGURE_RC=$rc"; worst=$((worst|rc)); [ $rc -eq 0 ] || exit $worst
   cmake --build build --parallel "$(nproc)"; rc=$?; echo "HARNESS_BUILD_RC=$rc"; worst=$((worst|rc)); exit $worst;;
 guards)  # the transcription guards a set must pass before any row is written (docs/runbook.md 3b + Tests)
   worst=0
   for g in "./build/tests/hven_bench_derivative_check" "./build/tests/hven_bench_bridge_oracle" "./build/tests/hven_bench_declared_point" "./scripts/check_cutest.sh" \
            "python3 -B schema/split.py --verify" "python3 -B schema/set_pin.py --verify" "python3 -B -m unittest discover -s tests"; do
     echo "=== $g"; $g > "$OUT/guard-$(echo "$g" | tr -c 'A-Za-z0-9\n' _).log" 2>&1; rc=$?; echo "GUARD_RC=$rc $g"; worst=$((worst|rc))
   done; exit $worst;;
 hs|cutest)
   if [ "$STEP" = hs ]; then CEIL=600; else CEIL=300; fi
   python3 -B runners/run_serial.py --problem-set "$STEP" --lanes hven-ipm,hven-sqp,ipopt,snopt --budget-wall-s "$CEIL" \
       --out "$OUT/run-$STEP" --solver-log-dir "$OUT/solver-logs-$STEP"
   rc=$?; echo "RUN_SERIAL_RC=$rc (0 = clean; 1 = a REFERENCE FAILURE with all rows written; anything else = the evidence is not whole)"
   exit $rc;;
 *) echo "unknown step"; exit 64;;
esac
