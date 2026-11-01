#!/usr/bin/env bash
# CHAIN 4 -- the brief's gates at the SHIPPED commit. Each leg its own lock
# acquisition; nothing killed on a failure.
S=/home/ghecht/Projects/hven/.scratch/m7w2m1
log() { echo "[$(date -Is)] $*" >> "$S/logs/chain4.log"; }
log "chain4 start at $(git -C /home/ghecht/Projects/hven rev-parse --short HEAD)"
"$S/run_locked.sh" debug4 "$S/stage_debug.sh" > "$S/logs/debug-shipped.log" 2>&1
log "debug rc=$?"
"$S/run_locked.sh" debug-long4 "$S/stage_debug_long.sh" > "$S/logs/debug-long-shipped.log" 2>&1
log "debug-long rc=$?"
"$S/run_locked.sh" gates4 "$S/stage_gates2.sh" > "$S/logs/gates-shipped.log" 2>&1
log "gates rc=$?"
CHANGED="src/drivers/sqp_solver.cpp src/drivers/sqp_options.cpp src/drivers/trace_writer.cpp \
 tests/drivers/test_callback.cpp tests/drivers/test_threads.cpp tests/drivers/test_options_dump.cpp \
 tests/sqp/test_trace_writer.cpp tests/sqp/test_qp_mode_sites.cpp tests/sqp/test_qp_cap_levers.cpp \
 tests/sqp/test_sqp_solver.cpp tests/sqp/test_continuation.cpp tests/sqp/test_hs_sweeps.cpp"
for cfg in Release Debug; do
  PSYM_CONFIG=$cfg "$S/run_locked.sh" "psym-$cfg" "$S/psym.sh" > "$S/logs/psym-shipped-$cfg.log" 2>&1
  log "psym-$cfg rc=$?"
  python3 -B "$S/classify_psym.py" --compare "$S/psym/compare-$cfg.txt" \
      --build "$S/psym-build" --changed-sources $CHANGED \
      > "$S/psym/classification-shipped-$cfg.txt" 2>&1
  log "classify-$cfg rc=$?"
done
log "chain4 done"
