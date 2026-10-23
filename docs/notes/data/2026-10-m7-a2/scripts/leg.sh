#!/bin/bash
# usage: leg.sh <logname> <command...>   -- one solver leg under the box lock, MKL_NUM_THREADS=1
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-aa38d0451de957235
L=$W/.scratch/a2/logs
mkdir -p "$L"
NAME=$1
shift
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
if pgrep -a hven_bench_cell; then echo PGREP_NONEMPTY; else echo PGREP_EMPTY; fi
echo FOREGROUND_START "$(date -Is)"
flock /tmp/box-build.lock "$@" > "$L/$NAME.log" 2>&1
rc=$?
echo FOREGROUND_END rc=$rc "$(date -Is)"
tail -5 "$L/$NAME.log"
exit $rc
