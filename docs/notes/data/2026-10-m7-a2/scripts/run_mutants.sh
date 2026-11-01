#!/bin/bash
# The A2 mutation run (mutants.py), from the worktree root.
set -o pipefail
cd /home/ghecht/Projects/hven/.claude/worktrees/agent-aa38d0451de957235 || exit 2
python3 -u .scratch/a2/mutants.py "$@"
