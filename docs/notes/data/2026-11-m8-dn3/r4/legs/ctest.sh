#!/usr/bin/env bash
# M8 DN-3 r4 (../../DECLARATION-r4.md): ../../legs/ctest.sh re-pointed (worktree agent-a739270f237b3e30c, build-flip/),
# TMPDIR on the home volume (r1's /tmp-quota lesson). TMPDIR is inside this worktree, so GIT_CEILING_DIRECTORIES stops
# git's repository discovery at it: BenchStampGuard.SelfTest's "no git" case builds a directory under TMPDIR that must
# not resolve to this worktree's repository (it did in the dev run: the one environment failure).
# usage: ctest.sh <build-dir-name> <log> [ctest args...]   Run as: flock /tmp/box-build.lock bash ctest.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a739270f237b3e30c
DIR=$WT/build-flip/${1:?dir}
LOG=${2:?log}
shift 2
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 TMPDIR=$WT/build-flip/tmp GIT_CEILING_DIRECTORIES=$WT/build-flip/tmp
mkdir -p "$TMPDIR"
cd "$DIR" || exit 1
setsid ctest -j4 --output-on-failure "$@" > "$LOG" 2>&1
rc=$?
tail -5 "$LOG"
echo "rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
