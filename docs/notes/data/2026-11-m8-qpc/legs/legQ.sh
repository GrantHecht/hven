#!/usr/bin/env bash
# Leg Q (DECLARATION-QPC section 6): SENS-Q under R and one arm with the independent certificate recomputation.
# Reuses leg N0's CSVs (same binaries, same cells) and adds the V-QP verdicts: every kOptimal exit under H is re-checked
# by the grid's independent recomputation (rows <= 10 ftol, the row-local product <= 100 ftol, no multiplier off the working
# set, the rounding-aware stationarity <= 10 opt_tol, signs <= opt_tol, all relative; the normwise stationarity and the
# unscaled product are REPORTED columns) and against the pinned optima in pinned_optima.tsv; ZERO failures under the arm
# (A = H, H-C1 or H-C2). R's failures are reported (they are what the round fixes). Then V-HONEST (cmp_sensq.py honest):
# every R kOptimal row passing V-QP stays kOptimal under the arm at the same objective; ZERO exceptions. And the M1
# reading (cmp_sensq.py m1, reported).
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
for alg in refactorize border; do
  python3 "$HERE/cmp_sensq.py" honest "$N0/R-$alg.csv" "$N0/A-$alg.csv" "$HERE/pinned_optima.tsv" > "$OUT/honest-$alg.txt" 2>&1; rc=$?
  echo "V-HONEST $alg rc=$rc: $(tail -1 "$OUT/honest-$alg.txt")"; [ $rc -gt $worst ] && worst=$rc
  python3 "$HERE/cmp_sensq.py" m1 "$N0/A-$alg.csv" > "$OUT/m1-A-$alg.txt" 2>&1
  echo "M1 A $alg: $(tail -1 "$OUT/m1-A-$alg.txt")"
done
echo "Q worst(arm)=$worst"; exit $worst
