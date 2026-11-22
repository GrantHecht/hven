#!/usr/bin/env bash
# RF-1 gate A+B2 leg P: re-run the TU-1 pilot's failing cells (pilot_cells.tsv: problem <TAB> option label, from the
# pilot store's results.csv) under one pilot lane binary, serial, one cell at a time on one pinned core,
# MKL_NUM_THREADS=1, iteration budget 300 (the pilot's), no wall deadline (a 900 s kill is reported, never a status).
# usage: pilot_run.sh <tag> <out dir> <core>
set -uo pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/rf1
L=$WT/docs/notes/data/2026-11-m8-rf1/legs
TAG=${1:?tag}; OUT=$(realpath -m "${2:?out}"); CORE=${3:?core}
BIN=$WT/.scratch/bin/pilot-$TAG/hven_bench_cell
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
mkdir -p "$OUT/run" "$OUT/cells"
cd "$OUT/run" || exit 1
echo "FOREGROUND_START $(date -Is) tag=$TAG core=$CORE bin=$(sha256sum "$BIN" | cut -c1-16)"
i=0
while IFS=$'\t' read -r -u 3 prob label; do
  i=$((i+1)); f="$OUT/cells/$(printf %03d $i)-$prob"
  [ -f "$f.out" ] && continue
  timeout 900 taskset -c "$CORE" "$BIN" --lane hven-sqp --problem "$prob" --options "$label" --budget-iters 300 \
    --options-dump "$f.opts" > "$f.tmp" 2> "$f.err"; rc=$?
  { echo "label=$label"; echo "rc=$rc"; cat "$f.tmp"; } > "$f.out"; rm -f "$f.tmp"
done 3< "$L/pilot_cells.tsv"
echo "FOREGROUND_END $(date -Is)"
