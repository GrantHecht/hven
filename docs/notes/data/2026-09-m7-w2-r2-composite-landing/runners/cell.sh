#!/usr/bin/env bash
# usage: cell.sh OUTPREFIX PROBLEM LABEL [env...]   (run INSIDE leg.sh; no timeout wrapper, by the box rule)
# argv = the harness's own hven-lane argv (runners/run_serial.py run_cell), budget 100 iterations.
set -o pipefail
out="$1"; prob="$2"; label="$3"; shift 3
cd /home/ghecht/Projects/hven/.scratch/m7w2r2/bench || exit 3
start=$(date +%s.%N)
env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 "$@" taskset -c 2 /home/ghecht/Projects/hven/.scratch/m7w2r2/bench/build/lanes/hven/hven_bench_cell \
  --lane hven-sqp --problem "$prob" --options "$label" --budget-iters 100 --options-dump "${out}.dump" --assert-reference > "${out}.out" 2> "${out}.err"
rc=$?; end=$(date +%s.%N); echo "cell $prob $label rc=$rc wall_s=$(echo "$end - $start" | bc)"; exit $rc
