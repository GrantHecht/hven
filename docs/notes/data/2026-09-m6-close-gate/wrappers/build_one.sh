#!/usr/bin/env bash
# Build ONE tree (optionally a target subset).  The build's status is CAPTURED
# before the trailing grep of the log runs and is the script's exit status.
#   usage: build_one.sh <abs-build-dir> <logtag> [extra cmake --build args]
set -o pipefail
source /home/ghecht/Projects/hven/.scratch/m6-close/env.sh
BLD="$1"; TAG="$2"; shift 2
CCACHE_DISABLE=1 cmake --build "$BLD" -j 8 "$@" > "$F/logs/build-$TAG.out" 2>&1
rc=$?
echo "BUILD_RC=$rc"
tail -2 "$F/logs/build-$TAG.out"
echo "error:/FAILED lines in the build log: $(grep -cE 'error:|FAILED' "$F/logs/build-$TAG.out")"
echo "BUILD_WORST_RC=$rc"
exit $rc
