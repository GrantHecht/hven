#!/usr/bin/env bash
# M8 AP-5 G2 r3 (../DECLARATION-r3.md): the 27 replay cells x (walk, ssn, ipm) on arm B3 (the r3 code head's corpus binary),
# compared with scripts/compare_replay.py EXACT against the COMMITTED r2 arm B rows (../replay/g2-<mode>-B.csv), then the ARMED leg:
# arm B3 again with a read-only counting callback + a never-requested token, EXACT against arm B3's own unarmed capture.
# usage: replay.sh <out-dir> <expected arm B3 sha256>     Run as: flock /tmp/box-build.lock bash replay.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a41fdd4ad89efc301
D=$WT/docs/notes/data/2026-11-m8-ap5-g2
OUT=${1:?out dir}
EXPECT=${2:?expected arm B3 sha256}
B=$WT/build-g2/armBbin3/hven_sqp_corpus
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
cd "$WT" || exit 1
mkdir -p "$OUT"
sha256sum "$B"
[ "$(sha256sum "$B" | cut -d' ' -f1)" = "$EXPECT" ] || { echo "BINARY_SHA_MISMATCH"; exit 4; }
CELLS=$(sed -n 3p docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)
echo "cells: $(echo "$CELLS" | tr ',' '\n' | wc -l)"
worst=0
for MODE in walk ssn ipm; do
  echo "RUN mode=$MODE arm=B3 $(date -Is)"
  "$B" --engine "$MODE" --cells "$CELLS" --csv "$OUT/r3-$MODE-B3.csv" > "$OUT/r3-run-$MODE-B3.txt" 2>&1
  rc=$?; echo "run rc=$rc"; [ $rc -gt $worst ] && worst=$rc
  python3 scripts/compare_replay.py "$D/replay/g2-$MODE-B.csv" "$OUT/r3-$MODE-B3.csv" "r3-vs-r2-$MODE" > "$OUT/r3-comparison-$MODE.txt" 2>&1
  rc=$?; echo "compare vs r2 rc=$rc"; tail -3 "$OUT/r3-comparison-$MODE.txt"; [ $rc -gt $worst ] && worst=$rc
done
for MODE in walk ssn ipm; do
  echo "RUN armed mode=$MODE arm=B3 $(date -Is)"
  HVEN_LEG_UNFIRED_TOKEN=1 "$B" --engine "$MODE" --cells "$CELLS" --callback count --csv "$OUT/r3-$MODE-B3armed.csv" > "$OUT/r3-run-$MODE-B3armed.txt" 2>&1
  rc=$?; echo "run rc=$rc"; [ $rc -gt $worst ] && worst=$rc
  python3 scripts/compare_replay.py "$OUT/r3-$MODE-B3.csv" "$OUT/r3-$MODE-B3armed.csv" "r3-armed-$MODE" > "$OUT/r3-comparison-armed-$MODE.txt" 2>&1
  rc=$?; echo "compare armed rc=$rc"; tail -3 "$OUT/r3-comparison-armed-$MODE.txt"; [ $rc -gt $worst ] && worst=$rc
done
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
