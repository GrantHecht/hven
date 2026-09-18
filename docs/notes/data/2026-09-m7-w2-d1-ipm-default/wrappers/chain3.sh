#!/usr/bin/env bash
# CHAIN 2 -- chain1 re-run from the fixed HEAD. Each leg its own lock
# acquisition; a failing leg does not stop the chain and nothing is killed.
# The Release leg runs FIRST and the stamped legs depend on its build, so its
# rc is the one to read before anything else in the transcript.
S=/home/ghecht/Projects/hven/.scratch/m7w2d1
log() { echo "[$(date -Is)] $*" >> "$S/logs/chain3.log"; }
log "chain3 start at $(git -C /home/ghecht/Projects/hven rev-parse --short HEAD)"
"$S/run_locked.sh" release-d1c "$S/stage_release.sh" > "$S/logs/release.log" 2>&1
log "release rc=$?"
"$S/run_locked.sh" legs-d1c "$S/stage_legs.sh" > "$S/logs/legs.log" 2>&1
log "legs rc=$?"
"$S/run_locked.sh" smoke-d1c "$S/stage_smoke.sh" > "$S/logs/smoke.log" 2>&1
log "smoke rc=$?"
"$S/run_locked.sh" debug-d1c "$S/stage_debug.sh" > "$S/logs/debug.log" 2>&1
log "debug rc=$?"
"$S/run_locked.sh" debug-long-d1c "$S/stage_debug_long.sh" > "$S/logs/debug-long.log" 2>&1
log "debug-long rc=$?"
log "chain3 done"
