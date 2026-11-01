#!/usr/bin/env bash
# usage: cell.sh OUTPREFIX PROBLEM LABEL [env...]   (run INSIDE leg.sh; no timeout wrapper, by the box rule)
set -o pipefail
out="$1"; prob="$2"; label="$3"; shift 3
cd /home/ghecht/Projects/hven-bench || exit 3
env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 "$@" taskset -c 2 /home/ghecht/Projects/hven/.scratch/m7w2e2d4/bench-build/lanes/hven/hven_bench_cell \
  --lane "${LANE:-hven-sqp}" --problem "$prob" --options "$label" --budget-iters "${BUDGET:-100}" --options-dump "${out}.dump" --assert-reference > "${out}.out" 2> "${out}.err"
rc=$?; echo "cell rc=$rc"; exit $rc
