#!/usr/bin/env bash
# RF-1c leg U: the TU-2 cutest cells of the four problems (BRITGAS, NET3, SAWPATH, YORKNET) on every SQP label of TU-2's arm grid
# (base labels default and sqp-qn x arms A0..A5 of tu/tu2_spec.json "sqp.arms" = 48 cells per arm), one lane binary, `--budget-facts 250000`
# (the cutest set's budget_facts) and `--budget-iters 300`. Counters, statuses and objectives only. 900 s per-cell kill reported, never a status.
# Cells are the lines of tu2_cells.tsv (name <TAB> problem <TAB> label); co-run at the width of the pinned physical cores given.
# usage: tu2_run.sh <tag> <out dir> <core> [<core> ...]    Run under box-lane.
set -uo pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/rf1c
L=$WT/docs/notes/data/2026-11-m8-rf1/legs-rf1c
R=/home/ghecht/Projects/hven/.scratch/rf1c-build-legs
TAG=${1:?tag}; OUT=$(realpath -m "${2:?out}"); shift 2; CORES=("$@")
BIN=$R/bin/tu2-$TAG/hven_bench_cell
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
mkdir -p "$OUT/cells"
cd "$R/hbt" || exit 1
echo "FOREGROUND_START $(date -Is) tag=$TAG cores=${CORES[*]} bin=$(sha256sum "$BIN" | cut -c1-16)"
i=0
while IFS=$'\t' read -r -u 3 name prob label; do
  f="$OUT/cells/$name"
  [ -f "$f.out" ] && continue
  core=${CORES[$((i % ${#CORES[@]}))]}
  ( timeout 900 taskset -c "$core" "$BIN" --lane hven-sqp --problem "$prob" --options "$label" --budget-iters 300 --budget-facts 250000 \
      --options-dump "$f.opts" > "$f.tmp" 2> "$f.err"; rc=$?
    { echo "label=$label"; echo "rc=$rc"; cat "$f.tmp"; } > "$f.out"; rm -f "$f.tmp" ) &
  i=$((i+1))
  (( i % ${#CORES[@]} == 0 )) && wait
done 3< "$L/tu2_cells.tsv"
wait
echo "FOREGROUND_END $(date -Is)"
