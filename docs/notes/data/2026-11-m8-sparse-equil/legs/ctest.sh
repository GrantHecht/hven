#!/usr/bin/env bash
# F1/P1: usage: ctest.sh <worktree> <build-dir-name> <log> [ctest args...]   Run under: flock /tmp/box-build.lock setsid -w bash ctest.sh ...
set -o pipefail
WT=${1:?worktree}; DIR=$WT/build-se/${2:?dir}; LOG=${3:?log}; shift 3
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 TMPDIR=$WT/build-se/tmp GIT_CEILING_DIRECTORIES=$WT/build-se/tmp
mkdir -p "$TMPDIR" "$(dirname "$LOG")"
cd "$DIR" || exit 1
setsid ctest -j4 --output-on-failure "$@" > "$LOG" 2>&1
rc=$?
tail -5 "$LOG"
echo "rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
