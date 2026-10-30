#!/usr/bin/env bash
# The U0 27-cell three-arm replay at HEAD, against the M6 close HEAD's committed
# CSVs. Invocation, environment pin and cell list as the M6 close gate ran them
# (docs/notes/data/2026-09-m6-close-gate/wrappers/replay_arm.sh).
set -uo pipefail
R=/home/ghecht/Projects/hven
S=$R/.scratch/m7w2m1
BIN=${HVEN_M1_REPLAY_BIN:-$S/rel/bench/hven_sqp_corpus}
D=${HVEN_M1_REPLAY_OUT:-$S/replay}
TAG=${HVEN_M1_REPLAY_TAG:-head}
mkdir -p "$D" "$S/logs"
CELLS="$(grep -v '^#' "$R/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt" | tr -d '\n')"
worst=0
for eng in walk ssn ipm; do
  MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
    "$BIN" --engine "$eng" --cells "$CELLS" --csv "$D/head-$eng.csv" \
    > "$S/logs/replay-$TAG-$eng.out" 2>&1
  rc=$?
  worst=$((worst | rc))
  echo "ran head-$eng rc=$rc (worst $worst)"
  [ $rc -eq 0 ] || { echo "REPLAY LEG FAILED: $eng"; exit $worst; }
done
echo "--- comparison against the M6 close HEAD CSVs ---"
BASE=$R/docs/notes/data/2026-09-m6-close-gate/replay
for eng in walk ssn ipm; do
  python3 -B "$R/scripts/compare_replay.py" "$BASE/head-$eng.csv" "$D/head-$eng.csv" \
      "M6 CLOSE HEAD-$eng vs M7 W2 M1 HEAD-$eng"
  rc=$?; worst=$((worst | rc)); echo "  CMP_RC=$rc (worst $worst)"
done
echo "REPLAY_WORST_RC=$worst"
exit $worst
