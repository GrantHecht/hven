#!/usr/bin/env bash
# M8 NaN-sticky gates: copied from ../../2026-11-m8-dn1d-singular/r3/legs/ and re-pointed (worktree nan, build-nan/, this directory).
# Run as: flock /tmp/box-build.lock bash ctest.sh ...   (ctest -j4 under setsid)
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/nan
DIR=$WT/build-nan/${1:?dir}
LOG=${2:?log}
shift 2
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1
cd "$DIR" || exit 1
setsid ctest -j4 --output-on-failure "$@" > "$LOG" 2>&1
rc=$?
tail -5 "$LOG"
echo "rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
