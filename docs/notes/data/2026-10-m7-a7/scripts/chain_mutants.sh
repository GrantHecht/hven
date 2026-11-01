#!/bin/bash
# The mutation run, then U0 OFF and ON at the committed head (each leg its own lock hold).
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a05b3c395d8efca12
S=$W/.scratch/a7
cd "$W" || exit 2
echo "CHAIN_MUT START $(date -Is) pid=$$" > "$S/logs/chain-mut.txt"
python3 "$S/mutants.py" >> "$S/logs/chain-mut.txt" 2>&1
echo "MUTANTS DONE $(date -Is) dirty=$(git -C "$W" status --porcelain --untracked-files=no | wc -l)" >> "$S/logs/chain-mut.txt"
bash "$S/u0.sh" off >> "$S/logs/chain-mut.txt" 2>&1
bash "$S/u0.sh" on >> "$S/logs/chain-mut.txt" 2>&1
echo "CHAIN_MUT END $(date -Is)" >> "$S/logs/chain-mut.txt"
