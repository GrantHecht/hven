#!/usr/bin/env bash
# PF-4 step 1 (measurement, not a gate leg): the scratch probe lane (branch pf4-probe 026373ee6, never pushed) on the profile
# cells, serial, one process at a time, core 2, MKL_NUM_THREADS=1. Per sync_borders batch the probe solves the batch's k border
# columns once as ONE multi-RHS solve and once as k single solves (perf_event instructions:u around each, steady_clock
# informational), and compares the multi-RHS columns bitwise against the single solves add_border took; at every sparse
# SymmetricFactor::analyze it records the pattern hash (repeat of the instance's last pattern / of any pattern seen in the process).
# Run as: setsid -w flock /tmp/box-build.lock bash step1_probe.sh <outdir>
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf4
OUT=$(realpath -m "${1:?outdir}"); mkdir -p "$OUT"
BIN=$WT/.scratch/bin/${PROBE_TAG:-probe}/hven_bench_cell   # probe 2: PROBE_TAG=probe2 (branch pf4-probe2 057f46358)
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 TMPDIR=$WT/.scratch/tmp
if [ -n "$(pgrep -x hven_bench_cell)" ]; then echo "PGREP_BUSY"; exit 3; fi
cd "$WT/.scratch/hb" || exit 1
echo "FOREGROUND_START $(date -Is)"; sha256sum "$BIN"
for P in ${CELLS:-ACOPP57 SMBANK DALLASM MESH READING6 DNIEPER CRESC50 MSS1 CRESC100 ACOPP118 BIGBANK YORKNET STEENBRC SPANHYD}; do
  echo "RUN $P $(date -Is)"
  taskset -c 2 "$BIN" --lane hven-sqp --problem "$P" --options default --budget-iters 0 --options-dump "$TMPDIR/opts-$P.txt" \
    > "$OUT/$P.stdout" 2> "$OUT/$P.stderr"
  echo "rc=$?"
done
echo "FOREGROUND_END $(date -Is)"
