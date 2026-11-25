#!/usr/bin/env bash
# M8 AP-5 G-RB: G6's legs/ctest.sh re-pointed (worktree g7, build-grb/); GIT_CEILING_DIRECTORIES = the worktree root.
# usage: ctest.sh <build-dir-name> <log> [ctest args...]     Run as: flock /tmp/box-build.lock bash ctest.sh ...   (ctest -j4 under setsid -w)
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/g-rb
DIR=$WT/build-grb/${1:?dir}
LOG=${2:?log}
shift 2
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 TMPDIR=$WT/build-grb/tmp GIT_CEILING_DIRECTORIES=$WT
mkdir -p "$TMPDIR"
cd "$DIR" || exit 1
setsid -w ctest -j4 --output-on-failure "$@" > "$LOG" 2>&1
rc=$?
tail -5 "$LOG"
echo "rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
