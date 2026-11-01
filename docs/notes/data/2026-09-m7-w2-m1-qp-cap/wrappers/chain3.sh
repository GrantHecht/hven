#!/usr/bin/env bash
# CHAIN 3 -- the fix round: the hs TRAIN read (both arms, prebuilt binaries),
# then the Release build and the whole Release suite at the fixed tree. Each leg
# takes the box lock on its own; the presets lane's cells interleave leg by leg.
S=/home/ghecht/Projects/hven/.scratch/m7w2m1
log() { echo "[$(date -Is)] $*" >> "$S/logs/chain3.log"; }
log "chain3 start at $(git -C /home/ghecht/Projects/hven rev-parse --short HEAD)"
"$S/run_locked.sh" hs-off "$S/stage_hs_cells.sh" off hs-off > "$S/logs/hs-off.log" 2>&1
log "hs-off rc=$?"
"$S/run_locked.sh" hs-budget "$S/stage_hs_cells.sh" budget hs-budget > "$S/logs/hs-budget.log" 2>&1
log "hs-budget rc=$?"
"$S/run_locked.sh" rel3 "$S/stage1.sh" > "$S/logs/stage1-fix.log" 2>&1
log "release rc=$?"
log "chain3 done"
