#!/usr/bin/env bash
# M8 walk cap (../DECLARATION.md, I1): ../../2026-11-m8-dn3/r4/legs/replay.sh re-pointed (this worktree, the frozen
# corpus binary build-wc/bin/hven_sqp_corpus), run three times, two comparisons per run:
#   I1-B  vs the base reference docs/notes/data/2026-10-m7-wx2-gates/gates/replay/head-<mode>.csv (which DN-3 r5 matched
#         with 0 differences on 030bae675's code), every difference on a declared mover cell of that mode
#         (movers-replay.txt; mover_check.py MOVERS_AS_DECLARED; an empty set means 0 differences)
#   I1-D  vs the declared dev run ../dev/replay/dev-<mode>.csv, 0 differences (the movers reproduce)
# compare_replay.py byte-exact (no --residual-gate).
# usage: replay.sh <out-dir> <corpus binary sha256>      Run as: flock /tmp/box-build.lock bash replay.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a7a2ce81cf8988de5
D=$WT/docs/notes/data/2026-11-m8-walk-cap
OUT=${1:?out dir}
EXPECT=${2:?corpus binary sha256}
BIN=$WT/build-wc/bin/hven_sqp_corpus
REF=$WT/docs/notes/data/2026-10-m7-wx2-gates/gates/replay
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 TMPDIR=$WT/build-wc/tmp
cd "$WT" || exit 1
mkdir -p "$OUT"
GOT=$(sha256sum "$BIN" | cut -d' ' -f1)
echo "binary $BIN sha256 $GOT"
[ "$GOT" = "$EXPECT" ] || { echo "BINARY_SHA_MISMATCH"; exit 4; }
CELLS=$(sed -n 3p docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)
KS=${REPLAY_KS:-1 2 3}
worst=0
for K in $KS; do
  for MODE in walk ssn ipm; do
    echo "RUN k=$K mode=$MODE $(date -Is)"
    "$BIN" --engine "$MODE" --cells "$CELLS" --csv "$OUT/k$K-$MODE.csv" > "$OUT/run-k$K-$MODE.txt" 2>&1
    rc=$?; echo "run rc=$rc"; [ $rc -gt $worst ] && worst=$rc
    python3 scripts/compare_replay.py "$REF/head-$MODE.csv" "$OUT/k$K-$MODE.csv" "I1-B-k$K-$MODE" > "$OUT/comparison-B-k$K-$MODE.txt" 2>&1
    head -1 "$OUT/comparison-B-k$K-$MODE.txt"
    MV=$(awk -v m="$MODE" '$1==m {print $2}' "$D/legs/movers-replay.txt" 2>/dev/null | paste -sd, -)
    python3 "$D/legs/mover_check.py" "$OUT/comparison-B-k$K-$MODE.txt" "${MV:-NONE}" > "$OUT/mover-check-B-k$K-$MODE.txt" 2>&1
    rc=$?; cat "$OUT/mover-check-B-k$K-$MODE.txt"; echo "mover_check rc=$rc"; [ $rc -gt $worst ] && worst=$rc
    if [ -f "$D/dev/replay/dev-$MODE.csv" ]; then
      python3 scripts/compare_replay.py "$D/dev/replay/dev-$MODE.csv" "$OUT/k$K-$MODE.csv" "I1-D-k$K-$MODE" > "$OUT/comparison-D-k$K-$MODE.txt" 2>&1
      rc=$?; head -1 "$OUT/comparison-D-k$K-$MODE.txt"; echo "compare D rc=$rc"; [ $rc -gt $worst ] && worst=$rc
    else
      echo "NO_DEV_REFERENCE (dev run)"
    fi
  done
done
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
