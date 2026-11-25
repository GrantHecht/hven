#!/usr/bin/env bash
# M8 PF-1, copied from PF-HB (which copied the B8 riders) and re-pointed (worktree, build-pf1e, this directory); was: AP-2 B8 riders (copied from B7 riders, re-pointed) (B5's instrument): every example (HVEN_BUILD_EXAMPLES=ON, Release) runs and exits 0.
# Run as: flock /tmp/box-build.lock bash examples.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-aa2c5c3a798557d0e
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1
worst=0
for EX in $(find "$WT/build-pf1e/rel/examples" -maxdepth 1 -type f -executable -name 'hven_example_*' | sort); do
  echo "=== $(basename "$EX")"
  "$EX" 2>&1 | tail -8
  rc=${PIPESTATUS[0]}
  echo "rc=$rc"
  [ $rc -gt $worst ] && worst=$rc
done
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
