#!/bin/bash
# Debug build, Debug suite (long test excluded), then the long test alone -- each its own lock hold.
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a05b3c395d8efca12
S=$W/.scratch/a7
LOG=$S/logs/chain-debug.txt
echo "CHAIN_DEBUG START $(date -Is) pid=$$ HEAD $(git -C "$W" rev-parse HEAD)" >> "$LOG"
if pgrep -a hven_bench_cell; then echo PGREP_NONEMPTY >> "$LOG"; else echo PGREP_EMPTY >> "$LOG"; fi
bash "$S/build.sh" dbg >> "$LOG" 2>&1 || { echo "CHAIN_DEBUG BUILD_FAILED $(date -Is)" >> "$LOG"; exit 1; }
fail=0
for s in dbg long; do
    if pgrep -a hven_bench_cell; then echo PGREP_NONEMPTY >> "$LOG"; else echo PGREP_EMPTY >> "$LOG"; fi
    bash "$S/suite.sh" "$s" >> "$LOG" 2>&1 || fail=1
    grep -E 'tests passed|tests failed' "$S/logs/suite-$s.log" >> "$LOG"
done
echo "CHAIN_DEBUG END fail=$fail $(date -Is)" >> "$LOG"
