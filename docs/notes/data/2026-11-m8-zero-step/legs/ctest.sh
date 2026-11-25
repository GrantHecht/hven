#!/usr/bin/env bash
# F1. usage: ctest.sh <build-dir-name> <log>   Run as: flock /tmp/box-build.lock bash ctest.sh ...
set -o pipefail
. "$(dirname "$0")/env.sh"
DIR=$B/${1:?dir}; LOG=${2:?log}
pgrep_guard
echo "FOREGROUND_START $(date -Is)"
mkdir -p "$TMPDIR"
cd "$DIR" || exit 1
setsid -w ctest -j4 --output-on-failure > "$LOG" 2>&1
rc=$?
tail -4 "$LOG"
echo "rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
