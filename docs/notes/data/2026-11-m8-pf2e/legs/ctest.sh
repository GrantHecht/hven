#!/usr/bin/env bash
# PF-4 P1: ../../2026-11-m8-wa1/legs/ctest.sh re-pointed (.scratch/build-pf2e/<dir>, TMPDIR in the worktree).
# usage: ctest.sh <build-dir-name> <log> [ctest args...]   Run under: setsid -w flock /tmp/box-build.lock bash ctest.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf2e
DIR=$WT/.scratch/build-pf2e/${1:?dir}; LOG=${2:?log}; shift 2
:
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 TMPDIR=$WT/.scratch/tmp GIT_CEILING_DIRECTORIES=$WT/.scratch/tmp
mkdir -p "$TMPDIR" "$(dirname "$LOG")"
cd "$DIR" || exit 1
setsid -w ctest -j4 --output-on-failure "$@" > "$LOG" 2>&1
rc=$?
tail -5 "$LOG"
echo "rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
