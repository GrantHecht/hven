#!/usr/bin/env bash
# M7 W2 M1 FIX ROUND 1 -- the P-SYM gate in BOTH configs plus the Release suite,
# each leg its own lock acquisition. Nothing is killed on a failure.
S=/home/ghecht/Projects/hven/.scratch/m7w2m1
BEFORE=${PSYM_BEFORE:?}
log() { echo "[$(date -Is)] $*" >> "$S/logs/chain-fix1b.log"; }
log "chain-fix1 start at $(git -C /home/ghecht/Projects/hven rev-parse --short HEAD), before=$BEFORE"
CHANGED="tests/sqp/test_qp_cap_levers.cpp"
for cfg in Release Debug; do
  PSYM_CONFIG=$cfg PSYM_BEFORE=$BEFORE "$S/run_locked.sh" "psym-fix1b-$cfg" "$S/psym_fix1.sh" \
      > "$S/logs/psym-fix1b-$cfg.log" 2>&1
  log "psym-$cfg rc=$?"
  python3 -B "$S/classify_psym.py" --compare "$S/psym-fix1b/compare-$cfg.txt" \
      --build "$S/psym-build-fix1b" --changed-sources $CHANGED \
      > "$S/psym-fix1b/classification-fix1b-$cfg.txt" 2>&1
  log "classify-$cfg rc=$?"
done
"$S/run_locked.sh" suite-fix1b "$S/stage_suite_fix1.sh" > "$S/logs/suite-fix1b.log" 2>&1
log "suite rc=$?"
log "chain-fix1 done"
