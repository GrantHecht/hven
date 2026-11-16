#!/usr/bin/env bash
# build-g6/, this directory); the reference rows are G2 r3's committed arm B3 rows.
# compared with scripts/compare_replay.py EXACT against the COMMITTED r2 arm B rows (../replay/g2-<mode>-B.csv), then the ARMED leg:
# arm B3 again with a read-only counting callback + a never-requested token, EXACT against arm B3's own unarmed capture.
#
# G4: the 27 replay cells x (walk, ssn, ipm) on arm B (this code head's corpus binary), compare_replay.py EXACT against
# ../../2026-11-m8-ap5-g2/replay/r3-<mode>-B3.csv; then the ARMED leg (--callback count + HVEN_LEG_UNFIRED_TOKEN=1,
# which arms every G4 poll), EXACT against arm B's own unarmed capture.
# usage: replay.sh <out-dir> <expected arm B sha256> <expected arm A sha256>     Run as: flock /tmp/box-build.lock bash replay.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/g6
OUT=${1:?out dir}
EXPECT=${2:?expected arm B sha256}
EXPECT_A=${3:?expected arm A sha256}
A=$WT/build-g6/armAbin/hven_sqp_corpus
B=$WT/build-g6/armBbin/hven_sqp_corpus
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
unset HVEN_LEG_UNFIRED_TOKEN HVEN_LEG_COUNT_CALLBACK
cd "$WT" || exit 1
mkdir -p "$OUT"
sha256sum "$A" "$B"
[ "$(sha256sum "$B" | cut -d' ' -f1)" = "$EXPECT" ] || { echo "BINARY_SHA_MISMATCH"; exit 4; }
[ "$(sha256sum "$A" | cut -d' ' -f1)" = "$EXPECT_A" ] || { echo "BINARY_SHA_MISMATCH_A"; exit 4; }
CELLS=$(sed -n 3p docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)
echo "cells: $(echo "$CELLS" | tr ',' '\n' | wc -l)"
worst=0
for MODE in walk ssn ipm; do
  echo "RUN mode=$MODE arm=A $(date -Is)"
  "$A" --engine "$MODE" --cells "$CELLS" --csv "$OUT/g6-$MODE-A.csv" > "$OUT/g6-run-$MODE-A.txt" 2>&1
  rc=$?; echo "run rc=$rc"; [ $rc -gt $worst ] && worst=$rc
  echo "RUN mode=$MODE arm=B $(date -Is)"
  "$B" --engine "$MODE" --cells "$CELLS" --csv "$OUT/g6-$MODE-B.csv" > "$OUT/g6-run-$MODE-B.txt" 2>&1
  rc=$?; echo "run rc=$rc"; [ $rc -gt $worst ] && worst=$rc
  python3 scripts/compare_replay.py "$OUT/g6-$MODE-A.csv" "$OUT/g6-$MODE-B.csv" "g6-A-vs-B-$MODE" > "$OUT/g6-comparison-$MODE.txt" 2>&1
  rc=$?; echo "compare A vs B rc=$rc"; tail -3 "$OUT/g6-comparison-$MODE.txt"; [ $rc -gt $worst ] && worst=$rc
done
for MODE in walk ssn ipm; do
  echo "RUN armed mode=$MODE arm=B $(date -Is)"
  HVEN_LEG_UNFIRED_TOKEN=1 "$B" --engine "$MODE" --cells "$CELLS" --callback count --csv "$OUT/g6-$MODE-Barmed.csv" > "$OUT/g6-run-$MODE-Barmed.txt" 2>&1
  rc=$?; echo "run rc=$rc"; [ $rc -gt $worst ] && worst=$rc
  python3 scripts/compare_replay.py "$OUT/g6-$MODE-B.csv" "$OUT/g6-$MODE-Barmed.csv" "g6-armed-$MODE" > "$OUT/g6-comparison-armed-$MODE.txt" 2>&1
  rc=$?; echo "compare armed rc=$rc"; tail -3 "$OUT/g6-comparison-armed-$MODE.txt"; [ $rc -gt $worst ] && worst=$rc
done
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
