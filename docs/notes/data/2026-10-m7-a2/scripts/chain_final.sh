#!/bin/bash
# At the committed head: both builds + the three suite legs, then the measurement chain.
set -o pipefail
S=/home/ghecht/Projects/hven/.claude/worktrees/agent-aa38d0451de957235/.scratch/a2
bash "$S/chain_build_suites.sh"
bash "$S/chain_meas.sh"
echo "CHAIN_FINAL_DONE $(date -Is)"
