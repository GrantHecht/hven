#!/bin/bash
# The A1 mutation run (mutants.py), from the worktree root.
set -o pipefail
cd /home/ghecht/Projects/hven/.claude/worktrees/agent-ad5e6e56c9da5d8a5 || exit 2
python3 -u .scratch/a1i/mutants.py "$@"
