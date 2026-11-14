#!/usr/bin/env bash
# M8 walk cap (../DECLARATION.md, F1): ../../2026-11-m8-dn3/r4/legs/ctest.sh re-pointed (this worktree, build-wc/).
# TMPDIR inside the worktree; GIT_CEILING_DIRECTORIES stops git's repository discovery at it (BenchStampGuard.SelfTest).
# usage: ctest.sh <build-dir-name> <log> [ctest args...]   Run as: flock /tmp/box-build.lock bash ctest.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a7a2ce81cf8988de5
DIR=$WT/build-wc/${1:?dir}
LOG=${2:?log}
shift 2
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 TMPDIR=$WT/build-wc/tmp GIT_CEILING_DIRECTORIES=$WT/build-wc/tmp
mkdir -p "$TMPDIR" "$(dirname "$LOG")"
cd "$DIR" || exit 1
setsid ctest -j4 --output-on-failure "$@" > "$LOG" 2>&1
rc=$?
tail -5 "$LOG"
echo "rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
