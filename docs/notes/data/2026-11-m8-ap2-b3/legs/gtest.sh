#!/usr/bin/env bash
# AP-2 B3: run one gtest binary with a filter.  usage: gtest.sh <binary> <suite name>
# Run as: flock /tmp/box-build.lock bash gtest.sh ...
set -o pipefail
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1
"${1:?binary}" --gtest_filter="${2:?suite}.*"
rc=$?
echo "rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
