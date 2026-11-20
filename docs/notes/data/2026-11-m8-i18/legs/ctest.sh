#!/usr/bin/env bash
# I18: i17's ctest.sh at the lane's width. usage: ctest.sh <worktree> <build-dir-name> <log> [ctest args]   run under: box-lane -- bash ctest.sh ...
set -o pipefail
WT=${1:?worktree}; DIR=$WT/${2:?dir}; LOG=$(realpath -m "${3:?log}"); shift 3
export TMPDIR=$WT/.scratch/tmp MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 GIT_CEILING_DIRECTORIES=$WT
mkdir -p "$TMPDIR" "$(dirname "$LOG")"
echo "FOREGROUND_START $(date -Is) lane=${BOX_LANE:-none}"
cd "$DIR" || exit 1
setsid -w ctest -j"${BOX_LANE_JOBS:-4}" --output-on-failure "$@" > "$LOG" 2>&1; rc=$?
tail -5 "$LOG"; echo "ctest rc=$rc"; echo "FOREGROUND_END $(date -Is)"; exit $rc
