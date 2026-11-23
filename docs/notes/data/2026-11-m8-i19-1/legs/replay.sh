#!/usr/bin/env bash
# I19-1 counter replay: the walk-cap round's replay (../../2026-11-m8-walk-cap/legs/replay.sh) pointed at a BASE
# corpus binary built from origin/m8 (fd1e318b5) and the HEAD corpus binary of this branch. The base is run once
# per mode; the head three times per mode, each compared byte-exact (scripts/compare_replay.py, no residual gate)
# against the base run. Run as: box-lane -- bash replay.sh <repo-root> <base-bin> <head-bin> <out-dir>
set -o pipefail
WT=${1:?repo root}
BASE=${2:?base corpus binary}
HEAD=${3:?head corpus binary}
OUT=${4:?out dir}
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
cd "$WT" || exit 1
mkdir -p "$OUT"
CELLS=$(sed -n 3p docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)
echo "base sha256 $(sha256sum "$BASE" | cut -d' ' -f1)"
echo "head sha256 $(sha256sum "$HEAD" | cut -d' ' -f1)"
echo "FOREGROUND_START $(date -Is)"
worst=0
for MODE in walk ssn ipm; do
  "$BASE" --engine "$MODE" --cells "$CELLS" --csv "$OUT/base-$MODE.csv" > "$OUT/run-base-$MODE.txt" 2>&1
  rc=$?; echo "base $MODE rc=$rc"; [ $rc -gt $worst ] && worst=$rc
done
for K in 1 2 3; do
  for MODE in walk ssn ipm; do
    "$HEAD" --engine "$MODE" --cells "$CELLS" --csv "$OUT/head-k$K-$MODE.csv" > "$OUT/run-head-k$K-$MODE.txt" 2>&1
    rc=$?; echo "head k=$K $MODE rc=$rc"; [ $rc -gt $worst ] && worst=$rc
    python3 scripts/compare_replay.py "$OUT/base-$MODE.csv" "$OUT/head-k$K-$MODE.csv" "I19-1-k$K-$MODE" > "$OUT/comparison-k$K-$MODE.txt" 2>&1
    rc=$?; head -n 1 "$OUT/comparison-k$K-$MODE.txt"; echo "compare rc=$rc"; [ $rc -gt $worst ] && worst=$rc
  done
done
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
