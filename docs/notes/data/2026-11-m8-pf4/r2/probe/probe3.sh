#!/usr/bin/env bash
# PF-4 r2 probe 3 (measurement, not a leg): per-column backward error batch vs singles, on both trajectories
# (LAND=batch = arm B's, LAND=single = arm A's), serial, core 2, MKL_NUM_THREADS=1.
# Run as: setsid -w flock /tmp/box-build.lock bash probe3.sh <outdir>
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf4
OUT=$(realpath -m "${1:?outdir}"); mkdir -p "$OUT"
BIN=$WT/.scratch/bin/probe3/hven_bench_cell
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 TMPDIR=$WT/.scratch/tmp
if [ -n "$(pgrep -x hven_bench_cell)" ]; then echo "PGREP_BUSY"; exit 3; fi
cd "$WT/.scratch/hb" || exit 1
echo "FOREGROUND_START $(date -Is)"; sha256sum "$BIN"
for spec in ${CELLS:-DALLASM:default STEENBRC:default ACOPP118:default SPANHYD:default SPANHYD:never-default}; do
  P=${spec%%:*}; L=${spec#*:}
  for land in batch single; do
    tag=$P.$L.$land
    echo "RUN $tag $(date -Is)"
    PF4P3_CSV=$OUT/$tag.csv PF4P3_LAND=$land timeout -s KILL 900 taskset -c 2 "$BIN" --lane hven-sqp --problem "$P" \
      --options "$L" --budget-iters 0 --options-dump "$TMPDIR/opts-$P.txt" > "$OUT/$tag.stdout" 2> "$OUT/$tag.stderr"
    echo "rc=$?"
  done
done
echo "FOREGROUND_END $(date -Is)"
