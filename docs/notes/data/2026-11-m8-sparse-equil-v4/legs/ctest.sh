#!/usr/bin/env bash
# sparse-equil v4: copied from ../../2026-11-m8-kopt-1x-r2/ (same relative path), re-pointed to this worktree and evidence directory; older provenance lines below are historical.
# F1/P1: usage: ctest.sh <worktree> <build-dir-name> <log> [ctest args...]   Run under: flock /tmp/box-build.lock setsid -w bash ctest.sh ...
set -o pipefail
WT=${1:?worktree}; DIR=$WT/build-k/${2:?dir}; LOG=${3:?log}; shift 3
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 TMPDIR=$WT/build-k/tmp GIT_CEILING_DIRECTORIES=$WT/build-k/tmp
mkdir -p "$TMPDIR" "$(dirname "$LOG")"
cd "$DIR" || exit 1
setsid ctest -j4 --output-on-failure "$@" > "$LOG" 2>&1
rc=$?
tail -5 "$LOG"
echo "rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
