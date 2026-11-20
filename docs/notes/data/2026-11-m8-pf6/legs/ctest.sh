#!/usr/bin/env bash
# usage: ctest.sh <worktree> <build-dir-name> <log> [ctest args]   Run as: flock /tmp/box-build.lock setsid -w bash ctest.sh ...
set -o pipefail
WT=${1:?worktree}; DIR=$WT/${2:?dir}; LOG=$(realpath -m "${3:?log}"); shift 3
export TMPDIR=$WT/.scratch/tmp MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 GIT_CEILING_DIRECTORIES=$WT
mkdir -p "$TMPDIR" "$(dirname "$LOG")"
echo echo "FOREGROUND_START $(date -Is)"
cd "$DIR" || exit 1
setsid -w ctest -j${BOX_LANE_JOBS:-4} --output-on-failure "$@" > "$LOG" 2>&1; rc=$?
tail -5 "$LOG"; echo "ctest rc=$rc"; echo "FOREGROUND_END $(date -Is)"; exit $rc
