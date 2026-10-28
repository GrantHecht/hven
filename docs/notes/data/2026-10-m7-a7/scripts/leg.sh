#!/bin/bash
# usage: leg.sh <logname> <command...>   -- one solver leg under the box lock, MKL_NUM_THREADS=1
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a05b3c395d8efca12
L=$W/.scratch/a7/logs
mkdir -p "$L"
NAME=$1
shift
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
echo FOREGROUND_START "$(date -Is)"
flock /tmp/box-build.lock bash -c 'if pgrep -a hven_bench_cell; then echo IN_LOCK_PGREP_NONEMPTY; else echo IN_LOCK_PGREP_EMPTY; fi; "$@"' leg "$@" > "$L/$NAME.log" 2>&1
rc=$?
echo FOREGROUND_END rc=$rc "$(date -Is)"
tail -3 "$L/$NAME.log"
exit $rc
