#!/usr/bin/env bash
# M8 NaN-sticky gates (../DECLARATION.txt): the pch half of ../../2026-11-m8-dn1d-singular/r3/legs/misc.sh, re-pointed
# (worktree nan, build-nan/). ipm_solver.cpp gains one include, appended after the PCH-mirrored list.
# usage: pch.sh <evidence-dir>   Run as: flock /tmp/box-build.lock bash pch.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/nan
EV=${1:?evidence dir}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1
cd "$WT" || exit 1
mkdir -p "$EV"
rm -rf "$WT/build-nan/pch-check"
HVEN_PCH_NEUTRALITY_JOBS=16 HVEN_PCH_NEUTRALITY_WORKDIR=$WT/build-nan/pch-check \
  bash scripts/check_pch_neutrality.sh > "$EV/pch-neutrality.txt" 2>&1
rc=$?; tail -6 "$EV/pch-neutrality.txt"
echo "rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
