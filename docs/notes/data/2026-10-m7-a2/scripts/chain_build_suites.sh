#!/bin/bash
# Release + Debug builds, then the three suite legs (each its own lock hold).
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-aa38d0451de957235
S=$W/.scratch/a2
echo "HEAD $(git -C "$W" rev-parse HEAD) DIRTY=$(git -C "$W" status --porcelain --untracked-files=no | wc -l)"
fail=0
bash "$S/build.sh" rel || fail=1
bash "$S/build.sh" dbg || fail=1
if [ "$fail" = 0 ]; then
    bash "$S/chain_suites.sh" || fail=1
fi
echo "CHAIN_BUILD_SUITES_DONE fail=$fail $(date -Is)"
