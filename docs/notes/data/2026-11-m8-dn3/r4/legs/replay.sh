#!/usr/bin/env bash
# M8 DN-3 r4 (../../DECLARATION-r4.md, I1): ../../legs/replay.sh re-pointed (worktree agent-a739270f237b3e30c, the flip
# tree's frozen corpus binary build-flip/flipbin/hven_sqp_corpus), run three times. The reference is unchanged: the
# 27-cell replay baseline docs/notes/data/2026-10-m7-wx2-gates/gates/replay/head-<mode>.csv, which r2's P2 matched at
# kNever with 0 differences (../../replay-r2/).
# usage: replay.sh <out-dir> <corpus binary sha256>      Run as: flock /tmp/box-build.lock bash replay.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a739270f237b3e30c
OUT=${1:?out dir}
EXPECT=${2:?corpus binary sha256}
BIN=$WT/build-flip/flipbin/hven_sqp_corpus
REF=$WT/docs/notes/data/2026-10-m7-wx2-gates/gates/replay
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 TMPDIR=$WT/build-flip/tmp
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
    "$BIN" --engine "$MODE" --cells "$CELLS" --csv "$OUT/r4-k$K-$MODE.csv" > "$OUT/r4-run-k$K-$MODE.txt" 2>&1
    rc=$?; echo "run rc=$rc"; [ $rc -gt $worst ] && worst=$rc
    python3 scripts/compare_replay.py "$REF/head-$MODE.csv" "$OUT/r4-k$K-$MODE.csv" "r4-k$K-$MODE" > "$OUT/r4-comparison-k$K-$MODE.txt" 2>&1
    rc=$?; echo "compare rc=$rc"; tail -3 "$OUT/r4-comparison-k$K-$MODE.txt"; [ $rc -gt $worst ] && worst=$rc
  done
done
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
