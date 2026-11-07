#!/usr/bin/env bash
# usage: run_ctest.sh <dir-name>   (full ctest, -j4)
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf1-riders
D=$WT/build-riders/${1:?dir}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo PGREP_BUSY; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1
cd "$D" && ctest -j4 --output-on-failure
rc=$?
echo "rc=$rc"; echo "FOREGROUND_END $(date -Is)"; exit $rc
