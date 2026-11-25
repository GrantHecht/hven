#!/usr/bin/env bash
# usage: run_test.sh <dir-name> <gtest args...>   (the riders walk-seam test binary)
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf1-riders
D=$WT/build-riders/${1:?dir}; shift
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo PGREP_BUSY; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1
"$D/tests/hven_qp_walk_seam_tests" "$@"
rc=$?
echo "rc=$rc"; echo "FOREGROUND_END $(date -Is)"; exit $rc
