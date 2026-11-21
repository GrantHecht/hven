#!/usr/bin/env bash
# I1: the replay cells (u0-replay-cells.txt line 3) x walk / ssn / ipm / interior on arm A and arm B, serial, one process at a
# time; scripts/compare_replay.py EXACT A vs B per engine. Wall-clock is not asserted.
# usage: replay.sh <out-dir> <armA sha256> <armB sha256>   Run as: flock /tmp/box-build.lock bash replay.sh ...
set -o pipefail
. "$(dirname "$0")/env.sh"
OUT=${1:?out dir}; SA=${2:?arm A sha256}; SB=${3:?arm B sha256}
A=$B/armAbin/hven_sqp_corpus; BB=$B/armBbin/hven_sqp_corpus
pgrep_guard
echo "FOREGROUND_START $(date -Is)"
[ "$(sha256sum "$A" | cut -d' ' -f1)" = "$SA" ] || { echo ARMA_SHA_MISMATCH; exit 4; }
[ "$(sha256sum "$BB" | cut -d' ' -f1)" = "$SB" ] || { echo ARMB_SHA_MISMATCH; exit 4; }
cd "$WT" || exit 1
mkdir -p "$OUT"
CELLS=$(sed -n 3p "$REPLAY_CELLS_FILE")
echo "cells: $(echo "$CELLS" | tr ',' '\n' | wc -l)"
worst=0
for MODE in walk ssn ipm interior; do
  for ARM in A B; do
    BIN=$A; [ $ARM = B ] && BIN=$BB
    echo "RUN mode=$MODE arm=$ARM $(date -Is)"
    "$BIN" --engine "$MODE" --cells "$CELLS" --csv "$OUT/$MODE-$ARM.csv" > "$OUT/run-$MODE-$ARM.txt" 2>&1
    rc=$?; echo "run rc=$rc"; [ $rc -gt $worst ] && worst=$rc
  done
  python3 scripts/compare_replay.py "$OUT/$MODE-A.csv" "$OUT/$MODE-B.csv" "zs-$MODE" > "$OUT/comparison-$MODE.txt" 2>&1
  rc=$?; echo "compare $MODE rc=$rc"; tail -2 "$OUT/comparison-$MODE.txt"; [ $rc -gt $worst ] && worst=$rc
done
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
