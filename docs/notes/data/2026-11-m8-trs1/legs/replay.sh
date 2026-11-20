#!/usr/bin/env bash
# usage: replay.sh <build dir> <out dir> <label>   27-cell corpus replay in walk, ssn, ipm; counters only.  Run under `box-lane --`.
set -o pipefail
BIN=${1:?build}/bench/hven_sqp_corpus; OUT=$(realpath -m "${2:?out}"); LABEL=${3:?label}
W=/home/ghecht/Projects/hven/.claude/worktrees/trs1
export TMPDIR=$W/.scratch/tmp MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
mkdir -p "$TMPDIR" "$OUT"; cd "$W" || exit 1
echo "binary $BIN sha256 $(sha256sum "$BIN" | cut -d' ' -f1)"
CELLS=$(sed -n 3p docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)
[ "$(echo "$CELLS" | tr ',' '\n' | wc -l)" -eq 27 ] || { echo CELL_COUNT_NOT_27; exit 4; }
worst=0
for MODE in walk ssn ipm; do
  "$BIN" --engine "$MODE" --cells "$CELLS" --csv "$OUT/$LABEL-$MODE.csv" > "$OUT/$LABEL-run-$MODE.txt" 2>&1
  rc=$?; echo "run $MODE rc=$rc"; [ $rc -gt $worst ] && worst=$rc
done
echo "worst=$worst"; exit $worst
