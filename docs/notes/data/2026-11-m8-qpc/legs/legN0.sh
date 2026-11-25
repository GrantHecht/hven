#!/usr/bin/env bash
# Leg N0 (DECLARATION-QPC section 6): SENS-Q at the walk seam, R vs H, member 0, both algebras.
# Every cell whose H row has converged_refinement_arms == 0 AND certificate_gate_refusals == 0 must equal R's row on every
# column but those two. TODO: bench/qp_dump_grid.cpp (to be built in both trees before freeze; prints per cell: file,
# algebra, status, every QpCounters field, objective as hexfloat, sha256 of x as hexfloats, the data-half reading and the
# independent KKT recomputation) and legs/cmp_sensq.py (the N0 comparator; also the V-QP verdicts for leg Q).
# usage: legN0.sh <H tree> <R tree> <out>      Run under: setsid ~/.local/bin/box-lane -- bash legN0.sh ...
set -o pipefail
HW=$(realpath "$1"); RW=$(realpath "$2"); OUT=$(realpath -m "$3"); mkdir -p "$OUT"
HERE=$(cd "$(dirname "$0")" && pwd)
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
echo "LEG N0 start $(date -Is) H=$(git -C $HW rev-parse HEAD) R=$(git -C $RW rev-parse HEAD)"
FIX=$HW/tests/qp/fixtures/e2-d4
for side in R H; do
  W=$RW; [ $side = H ] && W=$HW
  G=$W/.scratch/build-rel/bench/hven_qp_dump_grid
  [ -x "$G" ] || { echo "GRID_MISSING $side ($G)"; exit 4; }
  echo "$side grid sha256 $(sha256sum "$G" | cut -d' ' -f1)"
  for alg in refactorize border; do
    timeout 7200 "$G" --dir "$FIX" --exclude FORMAT.txt --algebra $alg --kill-s 900 --csv "$OUT/$side-$alg.csv" \
      > "$OUT/$side-$alg.log" 2>&1; echo "$side $alg rc=$?"
  done
done
worst=0
for alg in refactorize border; do
  python3 "$HERE/cmp_sensq.py" n0 "$OUT/R-$alg.csv" "$OUT/H-$alg.csv" > "$OUT/cmp-$alg.txt" 2>&1; rc=$?
  echo "N0 $alg rc=$rc: $(tail -1 "$OUT/cmp-$alg.txt")"; [ $rc -gt $worst ] && worst=$rc
done
echo "N0 worst=$worst"; echo "LEG N0 end $(date -Is)"; exit $worst
