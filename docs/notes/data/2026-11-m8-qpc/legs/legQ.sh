#!/usr/bin/env bash
# Leg Q (DECLARATION-QPC section 6): SENS-Q under R and H with the independent certificate recomputation.
# Reuses leg N0's CSVs (same binaries, same cells) and adds the V-QP verdicts: every kOptimal exit under H is re-checked
# by the grid's independent recomputation (rows <= 10 ftol, product <= 100 ftol, stationarity beyond rounding and signs
# <= opt_tol * scale) and against the pinned optima in pinned_optima.tsv; ZERO failures under H. R's failures are
# reported (they are what the round fixes). The unregularized-EQP comparison is reported, never a verdict.
# TODO (before freeze): cmp_sensq.py `vqp` mode and pinned_optima.tsv (PF1 -3.231904984786688e-03; QC8 qp7
# -92397.26984847; qp1 and entry6 from test_qp_implied_blocker.cpp; YORKNET qp25 -5.799578... ; the rest from the
# fixtures' own tests).
# usage: legQ.sh <N0 out dir> <out>
set -o pipefail
N0=$(realpath "$1"); OUT=$(realpath -m "$2"); mkdir -p "$OUT"
HERE=$(cd "$(dirname "$0")" && pwd)
worst=0
for side in R H; do for alg in refactorize border; do
  python3 "$HERE/cmp_sensq.py" vqp "$N0/$side-$alg.csv" "$HERE/pinned_optima.tsv" > "$OUT/vqp-$side-$alg.txt" 2>&1; rc=$?
  echo "V-QP $side $alg rc=$rc: $(tail -1 "$OUT/vqp-$side-$alg.txt")"
  [ $side = H ] && [ $rc -gt $worst ] && worst=$rc
done; done
echo "Q worst(H)=$worst"; exit $worst
