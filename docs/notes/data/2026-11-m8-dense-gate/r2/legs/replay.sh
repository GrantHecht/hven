#!/usr/bin/env bash
# M8 dense gate (../DECLARATION-r2.md, I1): DN-3 r4's legs/replay.sh re-pointed (this worktree, the frozen corpus binary
# build-dg/dgbin-r2/hven_sqp_corpus), three times. r2 reference: the walk cap ADDENDUM-r1 replay rows
# docs/notes/data/2026-11-m8-walk-cap/addendum-r1/replay/<mode>.csv (the base origin/m8 286fb5398 code).
# usage: replay.sh <out-dir> <corpus binary sha256>      Run as: flock /tmp/box-build.lock bash replay.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a44d4824b0beb0ea7
OUT=${1:?out dir}
EXPECT=${2:?corpus binary sha256}
BIN=$WT/build-dg/dgbin-r2/hven_sqp_corpus
REF=$WT/docs/notes/data/2026-11-m8-walk-cap/addendum-r1/replay
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 TMPDIR=$WT/build-dg/tmp
cd "$WT" || exit 1
mkdir -p "$OUT"
GOT=$(sha256sum "$BIN" | cut -d' ' -f1)
echo "binary $BIN sha256 $GOT"
[ "$GOT" = "$EXPECT" ] || { echo "BINARY_SHA_MISMATCH"; exit 4; }
CELLS=$(sed -n 3p docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)
worst=0
for K in 1 2 3; do
  for MODE in walk ssn ipm; do
    echo "RUN k=$K mode=$MODE $(date -Is)"
    "$BIN" --engine "$MODE" --cells "$CELLS" --csv "$OUT/dg-k$K-$MODE.csv" > "$OUT/dg-run-k$K-$MODE.txt" 2>&1
    rc=$?; echo "run rc=$rc"; [ $rc -gt $worst ] && worst=$rc
    python3 scripts/compare_replay.py "$REF/$MODE.csv" "$OUT/dg-k$K-$MODE.csv" "dg-k$K-$MODE" > "$OUT/dg-comparison-k$K-$MODE.txt" 2>&1
    rc=$?; echo "compare rc=$rc"; tail -3 "$OUT/dg-comparison-k$K-$MODE.txt"; [ $rc -gt $worst ] && worst=$rc
  done
done
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
