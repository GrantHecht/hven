#!/usr/bin/env bash
# R11 scratch: CUTEst TRAIN `default`, hven-sqp lane, the PF-1 P17 invocation, on the probe-instrumented pin.
set -o pipefail
B=/home/ghecht/Projects/hven/.claude/worktrees/r11/build-r11
H=$B/hven-bench-meas
cd "$H" || exit 1
[[ -z "$(pgrep -a hven_bench_cell)" ]] && echo PGREP_EMPTY || { echo PGREP_NOT_EMPTY; exit 3; }
echo "FOREGROUND_START run $(date -Is)"
export MKL_NUM_THREADS=1
rm -f "$B/train.probe"
R11_PROBE_OUT=$B/train.probe python3 runners/run_serial.py --set-version 'cutest-nlp-35 v1' --split TRAIN --budget-wall-s 300 --budget-iters 0 --options default --lanes hven-sqp --out "$B/train-out/default"
rc=$?
echo "FOREGROUND_END run $(date -Is) rc=$rc"
exit $rc
