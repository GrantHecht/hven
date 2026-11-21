#!/usr/bin/env bash
# PF-4 r2 probe 3j (measurement, not a leg): trajectory sensitivity of the mover cells to a unit-roundoff backward
# perturbation of the landed Schur columns (PF4P3_JITTER seed; 0 = none), on both landings (single = arm A, batch = arm B).
# Serial, core 2, MKL_NUM_THREADS=1. Run as: setsid -w flock /tmp/box-build.lock bash probe3j.sh <outdir>
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf4
OUT=$(realpath -m "${1:?outdir}"); mkdir -p "$OUT"
BIN=$WT/.scratch/bin/probe3j/hven_bench_cell
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 TMPDIR=$WT/.scratch/tmp
if [ -n "$(pgrep -x hven_bench_cell)" ]; then echo "PGREP_BUSY"; exit 3; fi
cd "$WT/.scratch/hb" || exit 1
echo "FOREGROUND_START $(date -Is)"; sha256sum "$BIN"
for spec in ${CELLS:-ACOPP118:default STEENBRB:default STEENBRD:default CRESC100:default MESH:never-default CRESC50:never-default}; do
  P=${spec%%:*}; L=${spec#*:}
  for land in single batch; do for seed in ${SEEDS:-0 1 2 3}; do
    tag=$P.$L.$land.j$seed
    [ -f "$OUT/$tag.rc" ] && continue
    PF4P3_LAND=$land PF4P3_JITTER=$seed timeout -s KILL 600 taskset -c 2 "$BIN" --lane hven-sqp --problem "$P" \
      --options "$L" --budget-iters 0 --options-dump "$TMPDIR/opts-$P.txt" > "$OUT/$tag.stdout" 2> "$OUT/$tag.stderr"
    echo $? > "$OUT/$tag.rc"
    echo "$tag rc=$(cat $OUT/$tag.rc) $(grep -E '^(status|iterations|side.qp_minor_iters)=' $OUT/$tag.stdout | tr '\n' ' ')"
  done; done
done
echo "FOREGROUND_END $(date -Is)"
