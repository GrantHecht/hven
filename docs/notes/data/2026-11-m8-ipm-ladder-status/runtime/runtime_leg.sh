#!/usr/bin/env bash
# M8 IPM ladder status (../DECLARATION.txt): copied from ../../2026-11-m8-nan-gates/ (legs/, runtime-r2/ -> runtime/) and re-pointed in code lines only (worktree ipmlad, build-ipmlad/, this directory, arm A); older provenance lines below are historical.
# M8 NaN-sticky gates r2 (../DECLARATION-r2.txt): copied from ../../2026-11-m8-dn1d-singular/r3/runtime/ and re-pointed (worktree nan, evidence this directory, arm A and arm B trees build-nan/, arm A = origin/m8 9c6059593); older provenance lines below are historical and untouched.
# M8 DN-1d r3 (../../DECLARATION-r3.txt): r2's copy of this file re-pointed (r3/, build-dn1de3/); older provenance lines below are historical.
# M8 DN-1d r2 (../../DECLARATION-r2.txt): r1's copy of this file re-pointed (r2/, build-dn1de2/); older provenance lines below are historical.
# M8 DN-1d (../DECLARATION.txt): copied from ../../2026-11-m8-dn1c-sqp-check/ and re-pointed (worktree dn1d, evidence this directory, arm A and arm B trees build-dn1de/, arm A 4776aa2ff rebuilt by legs/prebuild.sh); older provenance lines below are historical.
# M8 DN-1c (../DECLARATION.txt): copied from ../../2026-11-m8-dn1b-guard/ and re-pointed (worktree dn1c, evidence this directory, arm A and arm B trees build-dn1ce/, arm A 2199d1a2f rebuilt by legs/prebuild.sh); older provenance lines below are historical.
# M8 DN-1b (../DECLARATION.txt): copied from ../../2026-11-m8-dn1-impl/r2/ and re-pointed (worktree dn1b, evidence this directory, arm A and arm B trees build-dn1be/, arm A 7784e4f59 rebuilt by legs/prebuild.sh); older provenance lines below are historical.
# M8 DN-1 r2 (../../DECLARATION-r2.txt): copied from ../../runtime/ and re-pointed (evidence r2/runtime, arm B build-dn1r2/; arm A read-only from build-dn1e/).
# M8 DN-1 impl: copied from R11 (2026-11-m8-r11-block-decode) and re-pointed: worktree dn1, build-dn1e, this directory, arm A 02065dac.
# M8 R11: copied from F1 (2026-11-m8-f1-init; runtime from its runtime-r2, period 400000; census legs from PF-1) and re-pointed: worktree r11, build-r11e, this directory, arm A a8c540cf.
# M8 F1 RUNTIME r2 (../DECLARATION-r2.txt: run 1 VOID on a THROTTLE event; period 200000 -> 400000, evidence directory runtime-r2; nothing else). Was: M8 F1 (copied from PF-1, re-pointed: worktree f1, build-f1, this directory, arm A 2340b95d). Was: M8 PF-1, copied from PF-HB (which copied the B8 riders) and re-pointed (worktree, build-f1); was: AP-2 B8 runtime leg (B7 riders' copy) (the B2-runtime-r5 instrument) (DECLARATION.txt in this directory). ONE ENGINE MODE PER
# INVOCATION; run by run_all.sh under `flock /tmp/box-build.lock`, serial, alone.
#
# usage: runtime_r5_leg.sh <mode: walk|ssn|ipm|interior> <out-dir>
#
# Per measured process:
#   phase-align (wait until `date +%N` < 5e6), then
#   taskset -c 2 perf record -c 400000 -e instructions:u -o <data> --
#       perf stat -x, -e instructions:u -o <stat> -- <arm binary> <cell args>
#   then the VALIDITY CHECK: THROTTLE events (perf report --stats) must be 0 -- a
#   throttled process VOIDS the leg (it stops here, exit 5); then `perf report --comm
#   hven_sqp_corpus --sort symbol` into a text table, and the perf.data is deleted (the
#   table is the evidence).
# Rounds: r1 A->B, r2 B->A, r3 A->B. One untimed, unrecorded warm-up per arm first.
#
# B8 (DECLARATION.txt in this directory): B7 riders' instrument, unchanged (B7's interior-mode pin stays removed). Both
# arms run every mode at the defaults: arm A is bdd797e3 (origin/m8), so the interior rows of
# the two arms are counter-identical at the defaults and need no pre-event pin. Neither arm sees
# HVEN_LEG_PRE_B7_INFINITE_BOUND (unset by env -u); both carry the same placeholder variable
# HVEN_LEG_NOT_B7_INFINITE_BOUND=1 (read by nothing), so the two environments stay one size, as in B7's run.
set -o pipefail
MODE=${1:?walk|ssn|ipm|interior}
OUT=${2:?out dir}
WT=/home/ghecht/Projects/hven/.claude/worktrees/ipmlad
cd "$WT" || exit 1
BIN_A=$WT/build-ipmlad/armAbin/hven_sqp_corpus
BIN_B=$WT/build-ipmlad/armBbin/hven_sqp_corpus
CORE=2
CELLS=$(sed -n 3p docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)
SCRATCH=$WT/build-ipmlad/runtime-perf
mkdir -p "$OUT/syms" "$OUT/stat" "$OUT/raw" "$OUT/throttle" "$SCRATCH"
note() { echo "$1 $(date -Is)"; }
align() { # sub-second phase alignment: return once the nanosecond field is < 5e6
  while :; do ns=$(date +%N); [ $((10#$ns)) -lt 5000000 ] && return 0; done
}
bin_of() { [ "$1" = A ] && echo "$BIN_A" || echo "$BIN_B"; }
pin_of() { # the environment assignment every arm runs with (B7 riders: the placeholder, both arms, every mode)
  echo HVEN_LEG_NOT_B7_INFINITE_BOUND=1
}
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
  env -u HVEN_LEG_PRE_B7_INFINITE_BOUND MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 "$(pin_of "$arm")" taskset -c $CORE \
      perf record -q -c 400000 -e instructions:u -o "$data" -- \
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
note "LEG_START mode=$MODE core=$CORE period=400000 event=instructions:u"
sha256sum "$BIN_A" "$BIN_B"
first=$(echo "$CELLS" | cut -d, -f1)
for arm in A B; do # untimed, unrecorded warm-up
  bin=$(bin_of $arm)
  if [ "$MODE" = interior ]; then
    env -u HVEN_LEG_PRE_B7_INFINITE_BOUND MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 "$(pin_of "$arm")" taskset -c $CORE "$bin" --engine interior --cells "$CELLS" --csv "$SCRATCH/warmup-$arm.csv" > /dev/null 2>&1
  else
    env -u HVEN_LEG_PRE_B7_INFINITE_BOUND MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 "$(pin_of "$arm")" taskset -c $CORE "$bin" --internal-run-one "$first" --engine "$MODE" --internal-out "$SCRATCH/warmup-$arm.csv" > /dev/null 2>&1
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
