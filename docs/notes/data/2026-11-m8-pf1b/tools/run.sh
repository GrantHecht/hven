#!/usr/bin/env bash
# PF-1b dev wrapper (not evidence): box protocol around one command. usage: flock /tmp/box-build.lock bash run.sh <cmd...>
set -o pipefail
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo PGREP_BUSY; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1
"$@"
rc=$?
echo "rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
