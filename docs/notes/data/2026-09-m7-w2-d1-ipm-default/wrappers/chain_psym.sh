#!/usr/bin/env bash
# P-SYM in BOTH configs against the pre-event HEAD 33f68b1e, then the
# classification of every differing object.
S=/home/ghecht/Projects/hven/.scratch/m7w2d1
log() { echo "[$(date -Is)] $*" >> "$S/logs/psym.log"; }
CHANGED="src/drivers/ipm_options.cpp bench/bench_corpus.cpp bench/ipm_corpus_leg.cpp \
 tests/interior/test_ipm_solver_presets.cpp tests/interior/test_recovery_dispatch_gate.cpp \
 tests/interior/test_ipm_solver_entry.cpp tests/interior/test_ipm_stop_reason.cpp \
 tests/drivers/test_options_dump.cpp tests/drivers/test_solve_result.cpp \
 tests/sqp/test_corpus_cells.cpp"
log "psym start at $(git -C /home/ghecht/Projects/hven rev-parse --short HEAD)"
for cfg in Release Debug; do
  PSYM_CONFIG=$cfg "$S/run_locked.sh" "psym-$cfg" "$S/psym.sh" > "$S/logs/psym-$cfg.log" 2>&1
  log "psym-$cfg rc=$?"
  python3 -B "$S/classify_psym.py" --compare "$S/psym/compare-$cfg.txt" \
      --build "$S/psym-build" --changed-sources $CHANGED \
      > "$S/psym/classification-$cfg.txt" 2>&1
  log "classify-$cfg rc=$?"
done
log "psym done"
