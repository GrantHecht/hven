#!/bin/bash
# W3 master: build chain (already running, waited by pid) -> TRAIN build phase -> in-tree measurement chain -> TRAIN cells.
set -o pipefail
S=/home/ghecht/Projects/hven/.claude/worktrees/w3/.scratch/w3
BPID=$1
while kill -0 "$BPID" 2>/dev/null; do sleep 15; done
echo "MASTER build chain done $(date -Is)" >> $S/logs/master.txt
bash $S/scripts/train_chain.sh build >> $S/logs/train-chain.out 2>&1; echo "MASTER train build rc=$? $(date -Is)" >> $S/logs/master.txt
bash $S/scripts/chain_meas.sh >> $S/logs/chain-meas.out 2>&1; echo "MASTER meas rc=$? $(date -Is)" >> $S/logs/master.txt
bash $S/scripts/train_chain.sh cells >> $S/logs/train-chain.out 2>&1; echo "MASTER train cells rc=$? $(date -Is)" >> $S/logs/master.txt
