#!/usr/bin/env bash
# usage: replay.sh <worktree> <build-dir-name> <out-dir> <label>   The 27-cell corpus replay in walk, ssn, ipm; serial, counters only.
#   Run as: flock /tmp/box-build.lock setsid -w bash replay.sh ...
set -o pipefail
WT=${1:?worktree}; BIN=$WT/${2:?build}/bench/hven_sqp_corpus; OUT=$(realpath -m "${3:?out}"); LABEL=${4:?label}
export TMPDIR=$WT/.scratch/tmp MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
mkdir -p "$TMPDIR" "$OUT"
# (box-lane rule: co-running cells on the other lane are allowed; counters only)
echo PGREP_EMPTY; echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
echo "binary $BIN sha256 $(sha256sum "$BIN" | cut -d' ' -f1)"
CELLS=$(sed -n 3p docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)
[ "$(echo "$CELLS" | tr ',' '\n' | wc -l)" -eq 27 ] || { echo CELL_COUNT_NOT_27; exit 4; }
worst=0
for MODE in walk ssn ipm; do
  "$BIN" --engine "$MODE" --cells "$CELLS" --csv "$OUT/$LABEL-$MODE.csv" > "$OUT/$LABEL-run-$MODE.txt" 2>&1
  rc=$?; echo "run $MODE rc=$rc"; [ $rc -gt $worst ] && worst=$rc
done
echo "worst=$worst"; echo "FOREGROUND_END $(date -Is)"; exit $worst
