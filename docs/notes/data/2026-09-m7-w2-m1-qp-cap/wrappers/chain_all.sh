#!/usr/bin/env bash
# THE WHOLE REMAINING TASK, unattended, each leg its OWN lock acquisition so the
# other lane's sweep interleaves leg by leg. Nothing is killed on a failure: a
# leg records its rc and the chain goes on, so one bad leg does not cost the
# night. Order is load-bearing -- P-SYM detaches the worktree and refuses a dirty
# tree, and the arm builds make it dirty, so P-SYM runs first.
S=/home/ghecht/Projects/hven/.scratch/m7w2m1
log() { echo "[$(date -Is)] $*" >> "$S/logs/chain.log"; }

log "chain start at $(git -C /home/ghecht/Projects/hven rev-parse --short HEAD)"

"$S/run_locked.sh" debug "$S/stage_debug.sh" > "$S/logs/debug.log" 2>&1
log "debug rc=$?"
"$S/run_locked.sh" debug-long "$S/stage_debug_long.sh" > "$S/logs/debug-long.log" 2>&1
log "debug-long rc=$?"

"$S/run_locked.sh" gates "$S/stage_gates.sh" > "$S/logs/gates.log" 2>&1
log "gates rc=$?"

CHANGED="src/drivers/sqp_solver.cpp src/drivers/sqp_options.cpp src/drivers/trace_writer.cpp \
 tests/drivers/test_callback.cpp tests/drivers/test_threads.cpp tests/drivers/test_options_dump.cpp \
 tests/sqp/test_trace_writer.cpp tests/sqp/test_qp_mode_sites.cpp tests/sqp/test_qp_cap_levers.cpp"
for cfg in Release Debug; do
  PSYM_CONFIG=$cfg "$S/run_locked.sh" "psym-$cfg" "$S/psym.sh" > "$S/logs/psym-$cfg.log" 2>&1
  log "psym-$cfg rc=$?"
  python3 -B "$S/classify_psym.py" --compare "$S/psym/compare-$cfg.txt" \
      --build "$S/psym-build" --changed-sources $CHANGED \
      > "$S/psym/classification-$cfg.txt" 2>&1
  log "classify-$cfg rc=$?"
done

for arm in off budget retry both; do
  "$S/run_locked.sh" "build-$arm" "$S/build_arm.sh" "$arm" > "$S/logs/build-$arm.log" 2>&1
  log "build-$arm rc=$?"
done

for leg in "off off" "budget budget-r1" "retry retry-r1" "both both-r1" \
           "budget budget-r2" "retry retry-r2" "both both-r2"; do
  set -- $leg
  "$S/run_locked.sh" "cells-$2" "$S/stage_arm_cells.sh" "$1" "$2" > "$S/logs/cells-$2.log" 2>&1
  log "cells-$2 rc=$?"
done
log "chain done"
