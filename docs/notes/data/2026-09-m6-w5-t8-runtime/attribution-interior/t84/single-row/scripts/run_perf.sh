#!/bin/bash
# Driver: pgrep OUTSIDE the lock, then the perf batch UNDER the lock.
set -o pipefail
R=/home/ghecht/Projects/hven/.scratch/w5t89ra5
PASS=$1 ROUND=$2
pgrep -af 'cmake|ninja|ctest|hven_|codex|clang' > $R/logs/perf$PASS-r$ROUND.pgrep 2>/dev/null || echo PGREP_EMPTY > $R/logs/perf$PASS-r$ROUND.pgrep
flock /tmp/box-build.lock bash $R/leg_perf.sh $PASS $ROUND
rc=$?; echo "WRAPPER_EXIT=$rc"; exit $rc
