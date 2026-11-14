#!/usr/bin/env bash
# M8 AP-5 G1 (../DECLARATION.txt P3): rewritten from ../../2026-11-m8-dn3b/legs/replay.sh as an A/B leg -- the 27 replay
# cells x (walk, ssn, ipm) on the frozen arm A and arm B corpus binaries, compared with scripts/compare_replay.py EXACT.
# The 76-column SQP schema carries no stop-reason column, so the declared column map is the identity here.
# usage: replay.sh <out-dir> <expected arm B sha256>     Run as: flock /tmp/box-build.lock bash replay.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a41fdd4ad89efc301
OUT=${1:?out dir}
EXPECT=${2:?expected arm B sha256}
A=$WT/build-g2/armAbin/hven_sqp_corpus
B=$WT/build-g2/armBbin/hven_sqp_corpus
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
cd "$WT" || exit 1
mkdir -p "$OUT"
if [ ! -f "$B" ]; then mkdir -p "$(dirname "$B")"; cp -p "$WT/build-g2/rel/bench/hven_sqp_corpus" "$B" || exit 1; fi
sha256sum "$A" "$B"
[ "$(sha256sum "$B" | cut -d' ' -f1)" = "$EXPECT" ] || { echo "BINARY_SHA_MISMATCH"; exit 4; }
head -1 "$WT/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt" > /dev/null || exit 1
CELLS=$(sed -n 3p docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)
echo "cells: $(echo "$CELLS" | tr ',' '\n' | wc -l)"
worst=0
for MODE in walk ssn ipm; do
  for ARM in A B; do
    BIN=$A; [ $ARM = B ] && BIN=$B
    echo "RUN mode=$MODE arm=$ARM $(date -Is)"
    "$BIN" --engine "$MODE" --cells "$CELLS" --csv "$OUT/g2-$MODE-$ARM.csv" > "$OUT/g2-run-$MODE-$ARM.txt" 2>&1
    rc=$?; echo "run rc=$rc"; [ $rc -gt $worst ] && worst=$rc
  done
  python3 scripts/compare_replay.py "$OUT/g2-$MODE-A.csv" "$OUT/g2-$MODE-B.csv" "g2-$MODE" > "$OUT/g2-comparison-$MODE.txt" 2>&1
  rc=$?; echo "compare rc=$rc"; tail -3 "$OUT/g2-comparison-$MODE.txt"; [ $rc -gt $worst ] && worst=$rc
done
# THE ARMED LEG (design page section 3.5 proof 1): the same 27 cells on arm B again, with a read-only
# per-iteration callback installed (--callback count: the corpus's own counting observer) AND a stop token
# that is never requested (HVEN_LEG_UNFIRED_TOKEN=1: bench/corpus_cells.h corpus_leg_token), byte-compared with
# arm B's unarmed capture above by scripts/compare_replay.py EXACT.
for MODE in walk ssn ipm; do
  echo "RUN armed mode=$MODE arm=B $(date -Is)"
  HVEN_LEG_UNFIRED_TOKEN=1 "$B" --engine "$MODE" --cells "$CELLS" --callback count --csv "$OUT/g2-$MODE-Barmed.csv" > "$OUT/g2-run-$MODE-Barmed.txt" 2>&1
  rc=$?; echo "run rc=$rc"; [ $rc -gt $worst ] && worst=$rc
  python3 scripts/compare_replay.py "$OUT/g2-$MODE-B.csv" "$OUT/g2-$MODE-Barmed.csv" "g2-armed-$MODE" > "$OUT/g2-comparison-armed-$MODE.txt" 2>&1
  rc=$?; echo "compare armed rc=$rc"; tail -3 "$OUT/g2-comparison-armed-$MODE.txt"; [ $rc -gt $worst ] && worst=$rc
done
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
