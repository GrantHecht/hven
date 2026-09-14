#!/usr/bin/env bash
# The HEAD arm of the U0 27-cell three-engine replay.  The invocation, the
# environment pin and the cell list are the committed ones (the W5 T9 / W6
# T2-T7 shapes).  Every engine's status is OR-FOLDED; the arm exits with the
# worst.
set -uo pipefail
source /home/ghecht/Projects/hven/.scratch/m6-close/env.sh
ARM="$1"
# Overridable ONLY so the fold proof can stub a FAILING binary.
BIN=${HVEN_CLOSE_REPLAY_BIN:-$F/build-release/bench/hven_sqp_corpus}
D=$F/replay
mkdir -p "$D" "$F/logs"
CELLS="$(grep -v '^#' "$R/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt" | tr -d '\n')"
worst=0
for eng in walk ssn ipm; do
  MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
    "$BIN" --engine "$eng" --cells "$CELLS" --csv "$D/$ARM-$eng.csv" \
    > "$F/logs/replay-$ARM-$eng.out" 2>&1
  rc=$?
  worst=$((worst | rc))
  echo "ran $ARM-$eng rc=$rc  (worst $worst)"
  [ $rc -eq 0 ] || { echo "REPLAY LEG FAILED: $ARM $eng"; echo "REPLAY_ARM_WORST_RC=$worst"; exit $worst; }
done
echo "REPLAY_ARM_WORST_RC=$worst"
exit $worst
