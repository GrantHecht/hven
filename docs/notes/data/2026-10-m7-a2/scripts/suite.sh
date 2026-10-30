#!/bin/bash
# usage: suite.sh <rel|dbg|long>  -- one suite leg under the box lock (ctest -j4)
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-aa38d0451de957235
L=$W/.scratch/a2/logs
mkdir -p "$L"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
case "$1" in
    rel) ARGS="--test-dir $W/.scratch/a2/build-rel -j4 --no-tests=error --output-on-failure" ;;
    dbg) ARGS="--test-dir $W/.scratch/a2/build-dbg -j4 --no-tests=error -E ^Continuation.ProbeBudgetBoundsAFailingProposal$ --output-on-failure" ;;
    long) ARGS="--test-dir $W/.scratch/a2/build-dbg --no-tests=error -R ^Continuation.ProbeBudgetBoundsAFailingProposal$ --output-on-failure" ;;
    *) echo "bad arg"; exit 2 ;;
esac
if pgrep -a hven_bench_cell; then echo PGREP_NONEMPTY; else echo PGREP_EMPTY; fi
echo FOREGROUND_START "$(date -Is)"
flock /tmp/box-build.lock ctest $ARGS > "$L/suite-$1.log" 2>&1
rc=$?
echo FOREGROUND_END rc=$rc "$(date -Is)"
tail -8 "$L/suite-$1.log"
exit $rc
