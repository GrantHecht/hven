#!/usr/bin/env bash
# PF-4 r2 (../../DECLARATION-PF-4-r2.md): copy of ../../legs/ctest.sh re-pointed to the pf4-r2 worktree and r2/ (paths only); older lines are historical.
# PF-4 P1: ../../2026-11-m8-wa1/legs/ctest.sh re-pointed (.scratch/build-pf4/<dir>, TMPDIR in the worktree).
# usage: ctest.sh <build-dir-name> <log> [ctest args...]   Run under (r2): ~/.local/bin/box-lane -- bash ctest.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf4-r2
DIR=$WT/.scratch/build-pf4/${1:?dir}; LOG=${2:?log}; shift 2
[ -n "${BOX_LANE:-}" ] || { echo "NOT_IN_BOX_LANE (r2: every leg runs inside ~/.local/bin/box-lane)"; exit 3; }; echo "BOX_LANE=$BOX_LANE phys=[$BOX_LANE_PHYS] jobs=$BOX_LANE_JOBS"
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 TMPDIR=$WT/.scratch/tmp GIT_CEILING_DIRECTORIES=$WT/.scratch/tmp
mkdir -p "$TMPDIR" "$(dirname "$LOG")"
cd "$DIR" || exit 1
setsid -w ctest -j"${BOX_LANE_JOBS:-4}" --output-on-failure "$@" > "$LOG" 2>&1
rc=$?
tail -5 "$LOG"
echo "rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
