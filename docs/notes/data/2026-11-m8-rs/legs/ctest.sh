#!/usr/bin/env bash
# usage: ctest.sh <build-dir-name> <log> [extra ctest args]   Run as: flock /tmp/box-build.lock bash ctest.sh ...
set -o pipefail
source "$(dirname "$0")/env.sh"
box_check
DIR=$WT/${1:?dir}; LOG=$(realpath -m "${2:?log}"); shift 2
echo "FOREGROUND_START $(date -Is)"
cd "$DIR" || exit 1
setsid -w ctest -j4 --output-on-failure "$@" > "$LOG" 2>&1
rc=$?
tail -5 "$LOG"; echo "ctest rc=$rc"; echo "FOREGROUND_END $(date -Is)"
exit $rc
