#!/usr/bin/env bash
# M8 dense gate (../DECLARATION.md, F1): DN-3 r4's legs/ctest.sh re-pointed (this worktree, build-dg/). TMPDIR inside
# the worktree; GIT_CEILING_DIRECTORIES=$TMPDIR (BenchStampGuard.SelfTest's "no git" case).
# usage: ctest.sh <build-dir-name> <log> [ctest args...]   Run as: flock /tmp/box-build.lock bash ctest.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a44d4824b0beb0ea7
DIR=$WT/build-dg/${1:?dir}
LOG=${2:?log}
shift 2
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 TMPDIR=$WT/build-dg/tmp GIT_CEILING_DIRECTORIES=$WT/build-dg/tmp
mkdir -p "$TMPDIR"
cd "$DIR" || exit 1
setsid ctest -j4 --output-on-failure "$@" > "$LOG" 2>&1
rc=$?
tail -5 "$LOG"
echo "rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
