#!/usr/bin/env bash
# Leg N0 (DECLARATION-QPC section 6): SENS-Q at the walk seam, baseline grid vs arm grid, both algebras, one process per
# cell with a 900 s kill. Every cell whose arm row has converged_refinement_arms == 0 AND certificate_gate_refusals == 0
# must equal the baseline row on every column but those two (cmp_sensq.py n0). The grids are hven_qp_dump_grid builds
# (build_grid.sh) against the R and the arm installs; their sha256s must equal BINARIES.txt's "QPC grid" lines (checked by
# the caller's pre-flight). The cell list is the 67 dumps of tests/qp/fixtures/e2-d4 (FORMAT.txt excluded) of the H tree.
# usage: legN0.sh <R grid> <arm grid> <H tree> <out>      Run under: ~/.local/bin/box-lane -- bash legN0.sh ...
set -o pipefail
RG=$(realpath "$1"); AG=$(realpath "$2"); HW=$(realpath "$3"); OUT=$(realpath -m "$4"); mkdir -p "$OUT"
HERE=$(cd "$(dirname "$0")" && pwd)
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
echo "LEG N0 start $(date -Is) H=$(git -C "$HW" rev-parse HEAD) Rgrid=$(sha256sum "$RG" | cut -d' ' -f1) armgrid=$(sha256sum "$AG" | cut -d' ' -f1)"
FIX=$HW/tests/qp/fixtures/e2-d4
for side in R A; do
  G=$RG; [ $side = A ] && G=$AG
  for alg in refactorize border; do
    rm -f "$OUT/$side-$alg.csv"
    for f in "$FIX"/*.txt; do
      [ "$(basename "$f")" = FORMAT.txt ] && continue
      timeout 900 "$G" --file "$f" --algebra $alg --csv "$OUT/$side-$alg.csv" >> "$OUT/$side-$alg.log" 2>&1
      rc=$?; [ $rc -ne 0 ] && echo "CELL $side $alg $(basename "$f") rc=$rc (124 = killed)"
    done
    echo "$side $alg rows=$(($(wc -l < "$OUT/$side-$alg.csv") - 1))"
  done
done
worst=0
for alg in refactorize border; do
  python3 "$HERE/cmp_sensq.py" n0 "$OUT/R-$alg.csv" "$OUT/A-$alg.csv" > "$OUT/cmp-$alg.txt" 2>&1; rc=$?
  echo "N0 $alg rc=$rc: $(tail -1 "$OUT/cmp-$alg.txt")"; [ $rc -gt $worst ] && worst=$rc
done
echo "N0 worst=$worst"; echo "LEG N0 end $(date -Is)"; exit $worst
