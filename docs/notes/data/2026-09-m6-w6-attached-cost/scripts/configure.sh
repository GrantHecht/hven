#!/usr/bin/env bash
# Configure ONE fresh Release tree, EMPTY at start, at one absolute path.  The
# configure status is captured and is this script's exit status; the trailing
# tail cannot mask it.
set -o pipefail
source /home/ghecht/Projects/hven/.scratch/w6t6/env.sh
rm -rf "$B"
mkdir -p "$B" "$F/logs"
CCACHE_DISABLE=1 cmake -S "$R" -B "$B" "${UNIFORM[@]}" \
    -DCMAKE_BUILD_TYPE=Release > "$F/logs/configure.out" 2>&1
rc=$?
echo "CONFIGURE_RC=$rc"
tail -3 "$F/logs/configure.out"
echo "CONFIGURE_WORST_RC=$rc"
exit $rc
