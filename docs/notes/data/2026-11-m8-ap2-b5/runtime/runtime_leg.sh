#!/usr/bin/env bash
# AP-2 B5 runtime leg (the B2-runtime-r5 instrument) (DECLARATION.txt in this directory). ONE ENGINE MODE PER
# INVOCATION; run by run_all.sh under `flock /tmp/box-build.lock`, serial, alone.
#
# usage: runtime_r5_leg.sh <mode: walk|ssn|ipm|interior> <out-dir>
#
# Per measured process:
#   phase-align (wait until `date +%N` < 5e6), then
#   taskset -c 2 perf record -c 200000 -e instructions:u -o <data> --
#       perf stat -x, -e instructions:u -o <stat> -- <arm binary> <cell args>
#   then the VALIDITY CHECK: THROTTLE events (perf report --stats) must be 0 -- a
#   throttled process VOIDS the leg (it stops here, exit 5); then `perf report --comm
#   hven_sqp_corpus --sort symbol` into a text table, and the perf.data is deleted (the
#   table is the evidence).
# Rounds: r1 A->B, r2 B->A, r3 A->B. One untimed, unrecorded warm-up per arm first.
set -o pipefail
MODE=${1:?walk|ssn|ipm|interior}
OUT=${2:?out dir}
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a17ce5e51d9c90f96
cd "$WT" || exit 1
BIN_A=$WT/build-b5/armAbin/hven_sqp_corpus
BIN_B=$WT/build-b5/armBbin/hven_sqp_corpus
CORE=2
CELLS=$(sed -n 3p docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)
SCRATCH=$WT/build-b5/runtime-perf
mkdir -p "$OUT/syms" "$OUT/stat" "$OUT/raw" "$OUT/throttle" "$SCRATCH"
note() { echo "$1 $(date -Is)"; }
align() { # sub-second phase alignment: return once the nanosecond field is < 5e6
  while :; do ns=$(date +%N); [ $((10#$ns)) -lt 5000000 ] && return 0; done
}
bin_of() { [ "$1" = A ] && echo "$BIN_A" || echo "$BIN_B"; }
order_of() { case $1 in 1) echo "A B";; 2) echo "B A";; 3) echo "A B";; esac; }
worst=0
HERE=$(dirname "$(readlink -f "$0")")
measure() { # arm round tag -- cell-args...
  local arm=$1 r=$2 tag=$3; shift 3
  local bin; bin=$(bin_of "$arm")
  local base="$MODE-r$r-$arm-$tag"
  local data="$SCRATCH/$base.data"
  note "RUN mode=$MODE round=$r arm=$arm tag=$tag"
  align
  env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c $CORE \
      perf record -q -c 200000 -e instructions:u -o "$data" -- \
      perf stat -x, -e instructions:u -o "$OUT/stat/$base.txt" -- \
      "$bin" "$@" > "$OUT/raw/$base.stdout" 2> "$OUT/raw/$base.stderr"
  local rc=$?
  [ $rc -gt $worst ] && worst=$rc
  local thr
  thr=$(perf report -i "$data" --stats 2> /dev/null | grep -E '^ +THROTTLE events' | awk '{print $3}')
  echo "${thr:-0}" > "$OUT/throttle/$base.txt"
  if [ "${thr:-0}" != 0 ]; then
    note "THROTTLED mode=$MODE round=$r arm=$arm tag=$tag events=$thr -- LEG VOID"
    rm -f "$data"
    exit 5
  fi
  perf report -i "$data" --comm hven_sqp_corpus --sort symbol --stdio --no-children \
      -F overhead,sample,sym \
      2> /dev/null | grep -v '^#' | grep -v '^$' > "$OUT/syms/$base.txt"
  rm -f "$data"
  # r5: no per-process check; filter liveness is judged by analyze_r5.py for the interior mode only (informational per-process count kept here).
  python3 "$HERE/analyze_r5.py" --excluded-count "$OUT/syms/$base.txt" > "$OUT/syms/$base.excluded" || { note "PARSE_ERROR $base -- LEG VOID"; exit 6; }
  return $rc
}
note "LEG_START mode=$MODE core=$CORE period=200000 event=instructions:u"
sha256sum "$BIN_A" "$BIN_B"
first=$(echo "$CELLS" | cut -d, -f1)
for arm in A B; do # untimed, unrecorded warm-up
  bin=$(bin_of $arm)
  if [ "$MODE" = interior ]; then
    env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c $CORE "$bin" --engine interior --cells "$CELLS" --csv "$SCRATCH/warmup-$arm.csv" > /dev/null 2>&1
  else
    env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c $CORE "$bin" --internal-run-one "$first" --engine "$MODE" --internal-out "$SCRATCH/warmup-$arm.csv" > /dev/null 2>&1
  fi
  note "WARMUP arm=$arm rc=$?"
done
for r in 1 2 3; do
  if [ "$MODE" = interior ]; then
    for arm in $(order_of $r); do
      measure $arm $r all --engine interior --cells "$CELLS" --csv "$OUT/raw/$MODE-r$r-$arm-all.csv"
    done
  else
    for cell in $(echo "$CELLS" | tr ',' ' '); do
      for arm in $(order_of $r); do
        measure $arm $r "$cell" --internal-run-one "$cell" --engine "$MODE" --internal-out "$OUT/raw/$MODE-r$r-$arm-$cell.csv"
      done
    done
  fi
done
note "LEG_END mode=$MODE worst_rc=$worst"
exit $worst
