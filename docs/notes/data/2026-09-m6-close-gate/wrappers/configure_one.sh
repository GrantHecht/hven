#!/usr/bin/env bash
# Configure ONE build tree at an ABSOLUTE path, from EMPTY.  The configure's own
# status is CAPTURED before the trailing tail runs and is the script's exit
# status, so a successful tail cannot mask a failed configure.
#   usage: configure_one.sh <abs-build-dir> <Debug|Release> [extra cmake args]
set -o pipefail
source /home/ghecht/Projects/hven/.scratch/m6-close/env.sh
BLD="$1"; CFG="$2"; shift 2
TAG="$(basename "$BLD")"
rm -rf "$BLD"; mkdir -p "$BLD" "$F/logs"
CCACHE_DISABLE=1 cmake -S "$R" -B "$BLD" "${UNIFORM[@]}" \
    -DCMAKE_BUILD_TYPE="$CFG" "$@" > "$F/logs/configure-$TAG.out" 2>&1
rc=$?
echo "CONFIGURE_RC=$rc"
tail -3 "$F/logs/configure-$TAG.out"
grep -E 'EIGEN_MAX_ALIGN_BYTES resolved to' "$F/logs/configure-$TAG.out" || true
echo "CONFIGURE_WORST_RC=$rc"
exit $rc
