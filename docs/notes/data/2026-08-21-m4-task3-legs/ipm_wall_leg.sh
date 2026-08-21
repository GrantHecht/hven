#!/bin/bash
# IPM wall leg. The standing SQP bench does not reach InteriorPointSolver, so
# the retargeted evaluation path is measured directly here. Two arms:
#   serial   MKL_NUM_THREADS=1, pinned to one physical core -- the standing
#            protocol; isolates the code-path cost.
#   threaded default thread count, unpinned, machine otherwise idle -- the arm
#            in which the fill_rhs || fill_solver_coeffs overlap the legacy
#            entries had could have shown.
# base/head alternated per rep so machine drift lands on both sides.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
REPS=${1:-5}
INNER=${2:-9}
for arm in serial threaded; do
  echo "=== arm: $arm"
  for r in $(seq 1 "$REPS"); do
    for side in base head; do
      if [ "$arm" = serial ]; then
        out=$(MKL_NUM_THREADS=1 taskset -c 2 "$HERE/ipm_$side" "$INNER")
      else
        out=$("$HERE/ipm_$side" "$INNER")
      fi
      echo "$out" | sed "s/^/rep$r $side /"
    done
  done
done
