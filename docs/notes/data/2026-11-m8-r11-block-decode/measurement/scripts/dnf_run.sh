#!/usr/bin/env bash
# R11 scratch: the 13 deadline-bound census cells (killed at their wall budget by the corpus parent, so probe-v1 lost
# their counts) re-run on the probe-v2 binary (hit + 10 s tick snapshots), full budget, 7-wide pinned to physical cores 1-7.
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/r11
B=$WT/build-r11
BIN=$B/meas/bench/hven_sqp_corpus
O=$B/meas-out/dnf
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo PGREP_BUSY; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
rm -rf "$O"; mkdir -p "$O"
CELLS="f7_n10000_path_activity f7_n10000_path_corrupted f7_n10000_path_neutral f7_n10000_path_physics f7_n10000_path_warm f7_n20000_path_activity f7_n20000_path_corrupted f7_n20000_path_neutral f7_n20000_path_physics f7_n20000_path_warm f7_n5000_path_corrupted f7_n5000_path_neutral f7_n5000_path_warm"
set -- $CELLS
run_wave() {
  local core=1 pids=()
  for c in "$@"; do
    R11_PROBE_OUT=$O/dnf.probe taskset -c $core "$BIN" --engine walk --cells "$c" --csv "$O/$c.csv" > "$O/$c.log" 2>&1 &
    pids+=($!); core=$((core+1))
  done
  for p in "${pids[@]}"; do wait $p; echo "pid $p rc=$?"; done
}
run_wave $1 $2 $3 $4 $5 $6 $7
shift 7
run_wave "$@"
echo "FOREGROUND_END $(date -Is)"
