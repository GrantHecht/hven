#!/usr/bin/env bash
# M8 AP-5 G-S (../DECLARATION.md P3): G6's legs/replay.sh re-pointed (worktree g7, build-gs/, this directory). The 27 replay
# cells x (walk, ssn, ipm): arm A (origin/m8) vs arm B (the code head), scripts/compare_replay.py EXACT; then the ARMED leg
# (--callback count + HVEN_LEG_UNFIRED_TOKEN=1) on arm B, EXACT against arm B's own unarmed capture.
# usage: replay.sh <out-dir> <expected arm B sha256> <expected arm A sha256>     Run as: flock /tmp/box-build.lock bash replay.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/g-rb
OUT=${1:?out dir}
EXPECT=${2:?expected arm B sha256}
EXPECT_A=${3:?expected arm A sha256}
A=$WT/build-gs/armAbin/hven_sqp_corpus
B=$WT/build-gs/armBbin/hven_sqp_corpus
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 GIT_CEILING_DIRECTORIES=$WT
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
  "$A" --engine "$MODE" --cells "$CELLS" --csv "$OUT/r4-$MODE-A.csv" > "$OUT/r4-run-$MODE-A.txt" 2>&1
  rc=$?; echo "run rc=$rc"; [ $rc -gt $worst ] && worst=$rc
  echo "RUN mode=$MODE arm=B $(date -Is)"
  "$B" --engine "$MODE" --cells "$CELLS" --csv "$OUT/r4-$MODE-B.csv" > "$OUT/r4-run-$MODE-B.txt" 2>&1
  rc=$?; echo "run rc=$rc"; [ $rc -gt $worst ] && worst=$rc
  python3 scripts/compare_replay.py "$OUT/r4-$MODE-A.csv" "$OUT/r4-$MODE-B.csv" "r4-A-vs-B-$MODE" > "$OUT/r4-comparison-$MODE.txt" 2>&1
  rc=$?; echo "compare A vs B rc=$rc"; tail -3 "$OUT/r4-comparison-$MODE.txt"; [ $rc -gt $worst ] && worst=$rc
done
for MODE in walk ssn ipm; do
  echo "RUN armed mode=$MODE arm=B $(date -Is)"
  HVEN_LEG_UNFIRED_TOKEN=1 "$B" --engine "$MODE" --cells "$CELLS" --callback count --csv "$OUT/r4-$MODE-Barmed.csv" > "$OUT/r4-run-$MODE-Barmed.txt" 2>&1
  rc=$?; echo "run rc=$rc"; [ $rc -gt $worst ] && worst=$rc
  python3 scripts/compare_replay.py "$OUT/r4-$MODE-B.csv" "$OUT/r4-$MODE-Barmed.csv" "r4-armed-$MODE" > "$OUT/r4-comparison-armed-$MODE.txt" 2>&1
  rc=$?; echo "compare armed rc=$rc"; tail -3 "$OUT/r4-comparison-armed-$MODE.txt"; [ $rc -gt $worst ] && worst=$rc
done
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
