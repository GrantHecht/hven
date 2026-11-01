#!/bin/bash
set -o pipefail
S=/home/ghecht/Projects/hven/.claude/worktrees/agent-afecbe0a5611f9eff/.scratch/a3
bash $S/build.sh base rel
bash $S/build.sh base dbg
echo "CHAIN_BASE_DONE $(date -Is)"
