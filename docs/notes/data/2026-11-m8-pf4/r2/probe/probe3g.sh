#!/usr/bin/env bash
# PF-4 r2 probe 3g (measurement, not a leg): the mover cells with the GUARD landing (PF4P3_LAND=guard: k >= 2 batch columns
# with eta > 1e-12 get one keep-while-falling refinement step through solve_mat) and unit-roundoff jitter seeds.
# Counter-only: co-run inside one box-lane, one process per lane physical core ($BOX_LANE_PHYS), MKL_NUM_THREADS=1.
# Run as: ~/.local/bin/box-lane -- bash probe3g.sh <outdir>
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf4
OUT=$(realpath -m "${1:?outdir}"); mkdir -p "$OUT"
BIN=$WT/.scratch/bin/probe3g/hven_bench_cell
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 TMPDIR=$WT/.scratch/tmp
cd "$WT/.scratch/hb" || exit 1
echo "FOREGROUND_START $(date -Is) lane=$BOX_LANE phys=[$BOX_LANE_PHYS]"; sha256sum "$BIN"
jobs=()
for spec in ${CELLS:-ACOPP118:default STEENBRB:default STEENBRD:default CRESC100:default MESH:never-default CRESC50:never-default}; do
  for land in ${LANDS:-guard}; do for seed in ${SEEDS:-0 1 2}; do jobs+=("$spec:$land:$seed"); done; done
done
read -r -a cores <<< "${BOX_LANE_PHYS:-2}"
run_stream() { # slot
  local s=$1 i
  for ((i = s; i < ${#jobs[@]}; i += ${#cores[@]})); do
    IFS=: read -r P L land seed <<< "${jobs[$i]}"
    tag=$P.$L.$land.j$seed
    [ -f "$OUT/$tag.rc" ] && continue
    PF4P3_LAND=$land PF4P3_JITTER=$seed timeout -s KILL 900 taskset -c "${cores[$s]}" "$BIN" --lane hven-sqp --problem "$P" \
      --options "$L" --budget-iters 0 --options-dump "$TMPDIR/opts-$tag.txt" > "$OUT/$tag.stdout" 2> "$OUT/$tag.stderr"
    echo $? > "$OUT/$tag.rc"
    echo "$tag rc=$(cat $OUT/$tag.rc) $(grep -E '^(status|iterations|objective|side.qp_minor_iters)=' $OUT/$tag.stdout | tr '\n' ' ')"
  done
}
for ((s = 0; s < ${#cores[@]}; s++)); do run_stream $s & done
wait
echo "FOREGROUND_END $(date -Is)"
