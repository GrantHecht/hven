#!/usr/bin/env bash
# AP-2 B1 runtime-neutrality leg (DECLARATION.txt, B1-d). ONE ENGINE MODE PER
# INVOCATION, run under `flock /tmp/box-build.lock`, serial, alone.
#
# usage: runtime_leg.sh <mode: walk|ssn|ipm|interior> <events: A|B> <rounds> <out-dir>
#
#   arm A = the pin's hven_sqp_corpus  (build-b1/pin/hven_sqp_corpus, built from
#           34fba748 before any edit, copied byte for byte)
#   arm B = B1's hven_sqp_corpus       (build-b1/rel/bench/hven_sqp_corpus)
#
# walk|ssn|ipm : every one of the 27 replay cells is ONE process per arm
#                (--internal-run-one), so a perf stat counts one solve and not a
#                fork/exec parent; the arms are ALTERNATED at the cell (A then B,
#                back to back), three rounds.
# interior     : the top-level interior-point leg runs whole (--engine interior
#                --cells <27> --csv), the arms alternated within a round.
#
# One UNTIMED, UNRECORDED warm-up process per arm at the start of the batch, so
# no round is a batch's first. taskset -c 2 (SMT sibling cpu10 left idle),
# MKL_NUM_THREADS=OMP_NUM_THREADS=1. perf is user-space only (paranoid=2).
set -o pipefail
MODE=${1:?walk|ssn|ipm|interior}
PASS=${2:?A|B}
ROUNDS=${3:?rounds}
OUT=${4:?out dir}
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a70da1a77af98495c
cd "$WT" || exit 1
BIN_A=$WT/build-b1/pin/hven_sqp_corpus
BIN_B=$WT/build-b1/rel/bench/hven_sqp_corpus
CORE=2
case $PASS in
  A) EVENTS=instructions:u,branches:u,cycles:u,branch-misses:u,L1-icache-load-misses:u ;;
  B) EVENTS=instructions:u,cycles:u,de_dis_uop_queue_empty_di0:u,op_cache_hit_miss.op_cache_miss:u,op_cache_hit_miss.op_cache_hit:u,ic_fetch_stall.ic_stall_any:u ;;
  *) echo "pass must be A or B"; exit 2 ;;
esac
CELLS=$(sed -n 3p docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)
mkdir -p "$OUT/perf" "$OUT/raw"
worst=0
note() { echo "$1 $(date -Is)"; }
run_one() { # arm bin tag cell perf-out csv-out
  local arm=$1 bin=$2 tag=$3 cell=$4 pout=$5 cout=$6
  env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c $CORE \
      perf stat -x, -e $EVENTS -o "$pout" -- \
      "$bin" --internal-run-one "$cell" --engine "$MODE" --internal-out "$cout" \
      > "$cout.stdout" 2> "$cout.stderr"
  return $?
}
note "LEG_START mode=$MODE pass=$PASS rounds=$ROUNDS pin=taskset-$CORE events=$EVENTS"
sha256sum "$BIN_A" "$BIN_B"
if [ "$MODE" = interior ]; then
  # untimed warm-up of each arm
  for arm in A B; do
    bin=$BIN_A; [ $arm = B ] && bin=$BIN_B
    env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c $CORE "$bin" --engine interior --cells "$CELLS" --csv "$OUT/raw/warmup-$arm.csv" > /dev/null 2>&1
  done
  for r in $(seq 1 "$ROUNDS"); do
    for arm in A B; do
      bin=$BIN_A; [ $arm = B ] && bin=$BIN_B
      note "RUN mode=$MODE round=$r arm=$arm"
      env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c $CORE \
          perf stat -x, -e $EVENTS -o "$OUT/perf/$MODE-r$r-$arm-all.txt" -- \
          "$bin" --engine interior --cells "$CELLS" --csv "$OUT/raw/$MODE-r$r-$arm-all.csv" \
          > "$OUT/raw/$MODE-r$r-$arm-all.stdout" 2>&1
      rc=$?; [ $rc -gt $worst ] && worst=$rc
    done
  done
else
  first=$(echo "$CELLS" | cut -d, -f1)
  for arm in A B; do
    bin=$BIN_A; [ $arm = B ] && bin=$BIN_B
    run_one $arm "$bin" warmup "$first" /dev/null "$OUT/raw/warmup-$arm.csv"
  done
  for r in $(seq 1 "$ROUNDS"); do
    for cell in $(echo "$CELLS" | tr ',' ' '); do
      for arm in A B; do
        bin=$BIN_A; [ $arm = B ] && bin=$BIN_B
        note "RUN mode=$MODE round=$r cell=$cell arm=$arm"
        run_one $arm "$bin" "$MODE-r$r" "$cell" "$OUT/perf/$MODE-r$r-$arm-$cell.txt" "$OUT/raw/$MODE-r$r-$arm-$cell.csv"
        rc=$?; [ $rc -gt $worst ] && worst=$rc
      done
    done
  done
fi
note "LEG_END mode=$MODE worst_rc=$worst"
exit $worst
