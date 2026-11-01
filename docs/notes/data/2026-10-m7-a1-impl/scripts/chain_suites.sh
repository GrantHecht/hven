#!/bin/bash
# Release suite, Debug suite (long test excluded), then the long test alone -- each its own leg.
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-ad5e6e56c9da5d8a5
echo "HEAD $(git -C "$W" rev-parse HEAD) DIRTY=$(git -C "$W" status --porcelain --untracked-files=no | wc -l)"
fail=0
for s in rel dbg long; do
    bash "$W/.scratch/a1i/suite.sh" "$s" || fail=1
    grep -E 'tests passed|tests failed' "$W/.scratch/a1i/logs/suite-$s.log"
done
echo "SUITES_DONE fail=$fail $(date -Is)"
