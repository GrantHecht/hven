#!/usr/bin/env bash
# usage: ctest.sh <dir under build-pfe/> <log>   (ctest -j4 under setsid)
source "$(dirname "$0")/common.sh"
DIR=$BP/${1:?}; LOG=${2:?}
cd "$DIR" || fin 1
setsid ctest -j4 --output-on-failure > "$LOG" 2>&1
rc=$?
grep -E "tests passed|tests failed" "$LOG"
fin $rc
