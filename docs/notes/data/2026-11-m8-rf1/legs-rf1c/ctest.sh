#!/usr/bin/env bash
# PF-4 P1: ../../2026-11-m8-wa1/legs/ctest.sh re-pointed (.scratch/build-rf1/<dir>, TMPDIR in the worktree).
# usage: ctest.sh <build-dir-name> <log> [ctest args...]   Run under: setsid -w flock /tmp/box-build.lock bash ctest.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/rf1c
DIR=/home/ghecht/Projects/hven/.scratch/rf1c-build-legs/build-rf1/${1:?dir}; LOG=${2:?log}; shift 2
:
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 TMPDIR=/home/ghecht/Projects/hven/.scratch/rf1c-build-legs/tmp GIT_CEILING_DIRECTORIES=/home/ghecht/Projects/hven/.scratch/rf1c-build-legs/tmp
mkdir -p "$TMPDIR" "$(dirname "$LOG")"
cd "$DIR" || exit 1
setsid -w ctest -j4 --output-on-failure "$@" > "$LOG" 2>&1
rc=$?
tail -5 "$LOG"
echo "rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
