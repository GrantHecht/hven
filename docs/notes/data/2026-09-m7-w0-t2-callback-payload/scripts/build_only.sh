#!/usr/bin/env bash
# Build the bench target to completion.  The build's own status is CAPTURED
# before the trailing grep of the log runs, and the script exits with the
# captured value, so a grep that succeeds cannot mask a failed build.
set -o pipefail
source /home/ghecht/Projects/hven/.scratch/m7t2/env.sh
CCACHE_DISABLE=1 cmake --build "$B" -j 8 "$@" > "$F/logs/11-build.out" 2>&1
rc=$?
echo "BUILD_RC=$rc"
tail -3 "$F/logs/11-build.out"
echo "error:/FAILED lines in the build log: $(grep -cE 'error:|FAILED' "$F/logs/11-build.out")"
echo "BUILD_WORST_RC=$rc"
exit $rc
