#!/bin/bash
# usage: leg.sh <logname> <command...>   -- one build/solver/test leg under the box lock, MKL_NUM_THREADS=1
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/w3
L=$W/.scratch/w3/logs
mkdir -p "$L"
NAME=$1
shift
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
if pgrep -a hven_bench_cell; then echo "PGREP_NONEMPTY $NAME"; else echo "PGREP_EMPTY $NAME"; fi
flock /tmp/box-build.lock bash -c '
set -o pipefail
NAME=$1; LOG=$2; shift 2
if pgrep -a hven_bench_cell; then echo "IN_LOCK_PGREP_NONEMPTY $NAME"; else echo "IN_LOCK_PGREP_EMPTY $NAME"; fi
echo "FOREGROUND_START $NAME $(date -Is)"
"$@" > "$LOG" 2>&1
rc=$?
echo "FOREGROUND_END $NAME rc=$rc $(date -Is)"
exit $rc
' _ "$NAME" "$L/$NAME.log" "$@"
rc=$?
tail -3 "$L/$NAME.log"
exit $rc
