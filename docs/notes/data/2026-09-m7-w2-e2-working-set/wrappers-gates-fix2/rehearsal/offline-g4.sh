#!/usr/bin/env bash
# E2 fix2 offline G4 at d1edc391 (build/ Release): the U0 27-cell three-engine replay vs the M6 close pin.
set -o pipefail
ROOT=/home/ghecht/Projects/hven
D=$ROOT/.scratch/m7w2e2fix2s/g4; : > "$D/comparison.txt"
BIN=$ROOT/build/bench/hven_sqp_corpus
CELLS="$(grep -v '^#' "$ROOT/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt" | tr -d '\n')"
for eng in walk ssn ipm; do
  MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 "$BIN" --engine "$eng" --cells "$CELLS" --csv "$D/head-$eng.csv" > "$D/run-$eng.log" 2>&1
  echo "ran head-$eng rc=$?"
done
w=0
for eng in walk ssn ipm; do
  python3 -B "$ROOT/scripts/compare_replay.py" --residual-gate 1e-5 "$ROOT/docs/notes/data/2026-09-m6-close-gate/replay/head-$eng.csv" "$D/head-$eng.csv" \
    "M6 CLOSE HEAD-$eng vs E2 fix2 d1edc391 HEAD-$eng (GATED 1e-5)" | tee -a "$D/comparison.txt" | tail -4
  rc=${PIPESTATUS[0]}; echo "  CMP_RC=$rc"; [ $rc -eq 0 ] || w=1
done
exit $w
