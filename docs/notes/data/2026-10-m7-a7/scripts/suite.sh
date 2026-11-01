#!/bin/bash
# usage: suite.sh <rel|dbg|long> [extra ctest args]  -- one suite leg under the box lock (ctest -j4)
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a05b3c395d8efca12
L=$W/.scratch/a7/logs
mkdir -p "$L"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
K=$1
shift
case "$K" in
    rel) ARGS="--test-dir $W/.scratch/a7/build-rel -j4 --no-tests=error --output-on-failure" ;;
    dbg) ARGS="--test-dir $W/.scratch/a7/build-dbg -j4 --no-tests=error -E ^Continuation.ProbeBudgetBoundsAFailingProposal$ --output-on-failure" ;;
    long) ARGS="--test-dir $W/.scratch/a7/build-dbg --no-tests=error -R ^Continuation.ProbeBudgetBoundsAFailingProposal$ --output-on-failure" ;;
    *) echo "bad arg"; exit 2 ;;
esac
TAG=${SUITE_TAG:-$K}
echo FOREGROUND_START "$(date -Is)"
flock /tmp/box-build.lock bash -c "
set -o pipefail
if pgrep -a hven_bench_cell; then echo IN_LOCK_PGREP_NONEMPTY; else echo IN_LOCK_PGREP_EMPTY; fi
setsid -w ctest $ARGS $* > $L/suite-$TAG.log 2>&1
"
rc=$?
echo FOREGROUND_END rc=$rc "$(date -Is)"
tail -8 "$L/suite-$TAG.log"
exit $rc
