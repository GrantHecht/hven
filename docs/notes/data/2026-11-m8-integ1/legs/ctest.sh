#!/usr/bin/env bash
# usage: ctest.sh <build dir> <log>    Run under `box-lane --`.  ctest -j$BOX_LANE_JOBS --output-on-failure.
set -o pipefail
DIR=${1:?dir}; LOG=$(realpath -m "${2:?log}")
export TMPDIR=/home/ghecht/Projects/hven/.claude/worktrees/integ1/.scratch/tmp MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 GIT_CEILING_DIRECTORIES=/home/ghecht/Projects/hven/.claude/worktrees/integ1
mkdir -p "$TMPDIR" "$(dirname "$LOG")"
echo "FOREGROUND_START $(date -Is)"; cd "$DIR" || exit 1
ctest -N | tail -1 > "$LOG.count"
ctest -j${BOX_LANE_JOBS:-4} --output-on-failure > "$LOG" 2>&1; rc=$?
tail -4 "$LOG"; cat "$LOG.count"; echo "ctest rc=$rc"; echo "FOREGROUND_END $(date -Is)"; exit $rc
