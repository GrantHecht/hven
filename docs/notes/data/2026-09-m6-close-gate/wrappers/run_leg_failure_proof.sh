#!/usr/bin/env bash
# run_leg.sh's FAILURE-PROPAGATION proof, as its OWN leg.
# It cannot live inside fold_proof.sh: that script runs INSIDE
# /tmp/box-build.lock (it is itself invoked through run_leg.sh), and a nested
# run_leg.sh invocation blocks forever on the same lock -- observed and killed
# 2026-09-14 during W6 T6b.  So this runs standalone, takes the lock once for a
# deliberately failing nested pipeline, and asserts the wrapper's exit status.
set -uo pipefail
R=/home/ghecht/Projects/hven
F=$R/.scratch/m6-close
P=$F/foldproof
mkdir -p "$P"
fails=0
echo "=== run_leg.sh: a FAILING nested pipeline carries its status out of the lock ==="
echo "    (the pipeline's LAST command, tee, SUCCEEDS -- only pipefail inside the"
echo "     lock shell makes LEG_RC non-zero, and only the explicit carry-out makes"
echo "     the WRAPPER exit non-zero)"
bash "$F/run_leg.sh" "$P/run_leg-fail.log" bash -o pipefail -c 'false | tee /dev/null' \
    > "$P/run_leg-fail.stdout" 2>&1
rc=$?
if [ "$rc" -ne 0 ]; then
  echo "FOLD OK   run_leg.sh(failing nested pipeline) -> WRAPPER_RC=$rc (non-zero, as required)"
else
  echo "FOLD FAIL run_leg.sh(failing nested pipeline) -> WRAPPER_RC=0 (a failure was MASKED)"
  fails=1
fi
grep -E 'LEG_RC|WRAPPER_EXIT|PGREP' "$P/run_leg-fail.log"
echo "RUN_LEG_FAILURE_PROOF_FAILURES=$fails"
[ "$fails" -eq 0 ] || exit 1
echo "RUN_LEG_FAILURE_PROOF_WORST_RC=0"
exit 0
