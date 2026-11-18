#!/usr/bin/env bash
# usage: replay.sh <build-dir-name> <out-dir> [ref-dir]   Run as: flock /tmp/box-build.lock setsid -w bash replay.sh ...
# 27-cell replay x (walk, ssn, ipm) on the default route.  With ref-dir: compare_replay.py (EXACT, every non-wall column)
# against ref-dir/head-<mode>.csv.  Serial and solo (counters only; wall informational).
set -o pipefail
source "$(dirname "$0")/env.sh"
box_check
BIN=$WT/${1:?build}/bench/hven_sqp_corpus; OUT=$(realpath -m "${2:?out}"); REF=${3:-}
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$OUT"; sha256sum "$BIN"
CELLS=$(sed -n 3p docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)
worst=0
for MODE in walk ssn ipm; do
  "$BIN" --engine "$MODE" --cells "$CELLS" --csv "$OUT/head-$MODE.csv" > "$OUT/run-$MODE.txt" 2>&1
  rc=$?; echo "run $MODE rc=$rc"; [ $rc -gt $worst ] && worst=$rc
  if [ -n "$REF" ]; then
    python3 scripts/compare_replay.py "$REF/head-$MODE.csv" "$OUT/head-$MODE.csv" "$MODE" > "$OUT/comparison-$MODE.txt" 2>&1
    rc=$?; tail -2 "$OUT/comparison-$MODE.txt"; echo "compare $MODE rc=$rc"; [ $rc -gt $worst ] && worst=$rc
  fi
done
echo "worst=$worst"; echo "FOREGROUND_END $(date -Is)"
exit $worst
