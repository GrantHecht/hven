#!/usr/bin/env bash
# M8 AP-5 G4 (../DECLARATION.md): copied from ../../2026-11-m8-ap5-g2/legs-r3/replay.sh and re-pointed (this worktree,
# build-g4/, this directory); the reference rows are G2 r3's committed arm B3 rows.
# M8 AP-5 G2 r3 (../DECLARATION-r3.md): the 27 replay cells x (walk, ssn, ipm) on arm B3 (the r3 code head's corpus binary),
# compared with scripts/compare_replay.py EXACT against the COMMITTED r2 arm B rows (../replay/g2-<mode>-B.csv), then the ARMED leg:
# arm B3 again with a read-only counting callback + a never-requested token, EXACT against arm B3's own unarmed capture.
#
# G4: the 27 replay cells x (walk, ssn, ipm) on arm B (this code head's corpus binary), compare_replay.py EXACT against
# ../../2026-11-m8-ap5-g2/replay-r3/r3-<mode>-B3.csv; then the ARMED leg (--callback count + HVEN_LEG_UNFIRED_TOKEN=1,
# which arms every G4 poll), EXACT against arm B's own unarmed capture.
# usage: replay.sh <out-dir> <expected arm B sha256>     Run as: flock /tmp/box-build.lock bash replay.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a2559003d3a33fac4
REF=$WT/docs/notes/data/2026-11-m8-ap5-g2/replay-r3
OUT=${1:?out dir}
EXPECT=${2:?expected arm B sha256}
B=$WT/build-g4/armBbin/hven_sqp_corpus
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
unset HVEN_LEG_UNFIRED_TOKEN HVEN_LEG_COUNT_CALLBACK
cd "$WT" || exit 1
mkdir -p "$OUT"
sha256sum "$B"
[ "$(sha256sum "$B" | cut -d' ' -f1)" = "$EXPECT" ] || { echo "BINARY_SHA_MISMATCH"; exit 4; }
CELLS=$(sed -n 3p docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)
echo "cells: $(echo "$CELLS" | tr ',' '\n' | wc -l)"
worst=0
for MODE in walk ssn ipm; do
  echo "RUN mode=$MODE arm=B $(date -Is)"
  "$B" --engine "$MODE" --cells "$CELLS" --csv "$OUT/g4-$MODE-B.csv" > "$OUT/g4-run-$MODE-B.txt" 2>&1
  rc=$?; echo "run rc=$rc"; [ $rc -gt $worst ] && worst=$rc
  python3 scripts/compare_replay.py "$REF/r3-$MODE-B3.csv" "$OUT/g4-$MODE-B.csv" "g4-vs-g2r3-$MODE" > "$OUT/g4-comparison-$MODE.txt" 2>&1
  rc=$?; echo "compare vs g2 r3 rc=$rc"; tail -3 "$OUT/g4-comparison-$MODE.txt"; [ $rc -gt $worst ] && worst=$rc
done
for MODE in walk ssn ipm; do
  echo "RUN armed mode=$MODE arm=B $(date -Is)"
  HVEN_LEG_UNFIRED_TOKEN=1 "$B" --engine "$MODE" --cells "$CELLS" --callback count --csv "$OUT/g4-$MODE-Barmed.csv" > "$OUT/g4-run-$MODE-Barmed.txt" 2>&1
  rc=$?; echo "run rc=$rc"; [ $rc -gt $worst ] && worst=$rc
  python3 scripts/compare_replay.py "$OUT/g4-$MODE-B.csv" "$OUT/g4-$MODE-Barmed.csv" "g4-armed-$MODE" > "$OUT/g4-comparison-armed-$MODE.txt" 2>&1
  rc=$?; echo "compare armed rc=$rc"; tail -3 "$OUT/g4-comparison-armed-$MODE.txt"; [ $rc -gt $worst ] && worst=$rc
done
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
