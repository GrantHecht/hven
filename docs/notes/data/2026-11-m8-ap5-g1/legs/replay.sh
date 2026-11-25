#!/usr/bin/env bash
# M8 AP-5 G1 (../DECLARATION.txt P3): rewritten from ../../2026-11-m8-dn3b/legs/replay.sh as an A/B leg -- the 27 replay
# cells x (walk, ssn, ipm) on the frozen arm A and arm B corpus binaries, compared with scripts/compare_replay.py EXACT.
# The 76-column SQP schema carries no stop-reason column, so the declared column map is the identity here.
# usage: replay.sh <out-dir> <expected arm B sha256>     Run as: flock /tmp/box-build.lock bash replay.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-afb4015ceb50aa1d1
OUT=${1:?out dir}
EXPECT=${2:?expected arm B sha256}
A=$WT/build-g1/armAbin/hven_sqp_corpus
B=$WT/build-g1/armBbin/hven_sqp_corpus
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
cd "$WT" || exit 1
mkdir -p "$OUT"
if [ ! -f "$B" ]; then mkdir -p "$(dirname "$B")"; cp -p "$WT/build-g1/rel/bench/hven_sqp_corpus" "$B" || exit 1; fi
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
    "$BIN" --engine "$MODE" --cells "$CELLS" --csv "$OUT/g1-$MODE-$ARM.csv" > "$OUT/g1-run-$MODE-$ARM.txt" 2>&1
    rc=$?; echo "run rc=$rc"; [ $rc -gt $worst ] && worst=$rc
  done
  python3 scripts/compare_replay.py "$OUT/g1-$MODE-A.csv" "$OUT/g1-$MODE-B.csv" "g1-$MODE" > "$OUT/g1-comparison-$MODE.txt" 2>&1
  rc=$?; echo "compare rc=$rc"; tail -3 "$OUT/g1-comparison-$MODE.txt"; [ $rc -gt $worst ] && worst=$rc
done
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
