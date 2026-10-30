#!/usr/bin/env bash
# CHAIN 2 -- the shipped-default verification: Release build + suite, Debug build
# + suite, the long Debug test alone. Each leg takes the box lock on its own, so
# the other lane's cells interleave leg by leg. Nothing is killed on a failure.
S=/home/ghecht/Projects/hven/.scratch/m7w2m1
log() { echo "[$(date -Is)] $*" >> "$S/logs/chain2.log"; }
log "chain2 start at $(git -C /home/ghecht/Projects/hven rev-parse --short HEAD) (shipped default qp_failure_budget=3)"
"$S/run_locked.sh" rel2 "$S/stage1.sh" > "$S/logs/stage1-shipped.log" 2>&1
log "release rc=$?"
"$S/run_locked.sh" debug2 "$S/stage_debug.sh" > "$S/logs/debug-shipped.log" 2>&1
log "debug rc=$?"
"$S/run_locked.sh" debug-long2 "$S/stage_debug_long.sh" > "$S/logs/debug-long-shipped.log" 2>&1
log "debug-long rc=$?"
log "chain2 done"
