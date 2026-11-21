#!/usr/bin/env bash
# M8 AP-5 G3 (../DECLARATION-r3.md): adapted from ../../2026-11-m8-ap5-g2/ (legs-r3, runtime); see the declaration for what differs.
# Run as: flock /tmp/box-build.lock bash ctest.sh ...   (ctest -j4 under setsid)
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-ac15f1e91e3a3b6b2
DIR=$WT/build-g3/${1:?dir}
LOG=${2:?log}
shift 2
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 TMPDIR=$WT/build-g3/tmp GIT_CEILING_DIRECTORIES=$WT/build-g3
mkdir -p "$TMPDIR"
cd "$DIR" || exit 1
setsid ctest -j4 --output-on-failure "$@" > "$LOG" 2>&1
rc=$?
tail -5 "$LOG"
echo "rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
