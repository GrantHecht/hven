#!/usr/bin/env bash
# Leg Q (DECLARATION-QPC section 6): SENS-Q under R and one arm with the independent certificate recomputation.
# Reuses leg N0's CSVs (same binaries, same cells) and adds the V-QP verdicts: every kOptimal exit under H is re-checked
# by the grid's independent recomputation (rows <= 10 ftol, product <= 100 ftol, free-block stationarity and the
# rounding-aware stationarity <= 10 opt_tol, signs <= opt_tol, all relative) and against the pinned optima in pinned_optima.tsv; ZERO failures under the arm (A = H, H-C1 or H-C2). R's failures are
# reported (they are what the round fixes). The unregularized-EQP comparison is reported, never a verdict.
# Pinned optima: pinned_optima.tsv (each with the test that pins it).
# usage: legQ.sh <N0 out dir> <out>
set -o pipefail
N0=$(realpath "$1"); OUT=$(realpath -m "$2"); mkdir -p "$OUT"
HERE=$(cd "$(dirname "$0")" && pwd)
worst=0
for side in R A; do for alg in refactorize border; do
  python3 "$HERE/cmp_sensq.py" vqp "$N0/$side-$alg.csv" "$HERE/pinned_optima.tsv" > "$OUT/vqp-$side-$alg.txt" 2>&1; rc=$?
  echo "V-QP $side $alg rc=$rc: $(tail -1 "$OUT/vqp-$side-$alg.txt")"
  [ $side = A ] && [ $rc -gt $worst ] && worst=$rc
done; done
echo "Q worst(arm)=$worst"; exit $worst
