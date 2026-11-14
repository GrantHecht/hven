#!/usr/bin/env bash
# AP-2 B3-c: every example (HVEN_BUILD_EXAMPLES=ON, Release) runs and exits 0.
# Run as: flock /tmp/box-build.lock bash examples.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a3b067b098f5e9221
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1
worst=0
for EX in $(find "$WT/build-b3/rel/examples" -maxdepth 1 -type f -executable -name 'hven_example_*' | sort); do
  echo "=== $(basename "$EX")"
  "$EX" 2>&1 | tail -8
  rc=${PIPESTATUS[0]}
  echo "rc=$rc"
  [ $rc -gt $worst ] && worst=$rc
done
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
