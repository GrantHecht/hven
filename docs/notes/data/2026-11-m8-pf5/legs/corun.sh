#!/usr/bin/env bash
# PF-5 (../DECLARATION-PF-5.md): the co-run of the TRAIN and nlp-35 legs, CLAUDE.md section 7's terms: ONE cell process
# per pinned PHYSICAL core of the box lane ($BOX_LANE_PHYS, SMT siblings idle), MKL_NUM_THREADS=1 in every process,
# each cell through cell.sh MODE stat (perf stat -e instructions:u,cycles:u) at THREADS=1 pinned to its own core,
# CAP 300 s (a CAPPED row fails its check; deadline-class cells are not in either population). Only deterministic row
# columns and per-process instructions:u are asserted; wall-clock is informational and never quoted.
# A row that deviates under this co-run is a candidate: it is re-run ALONE (cell.sh, the lane's first core) before it
# is called a deviation (rerun mode).
# usage: corun.sh <jobfile> <binA> <shaA> <binB> <shaB>   jobfile lines: "<A|B> <lane> <problem> <label> <outprefix>"
#   Run as ONE step: ~/.local/bin/box-lane -- bash corun.sh ...
set -uo pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf5b
L=$WT/docs/notes/data/2026-11-m8-pf5/legs
JOBS=${1:?}; BA=${2:?}; SA=${3:?}; BB=${4:?}; SB=${5:?}
export TMPDIR=$WT/.scratch/tmp MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
[ -n "${BOX_LANE:-}" ] || { echo "NOT_IN_BOX_LANE"; exit 3; }; echo "BOX_LANE=$BOX_LANE phys=[$BOX_LANE_PHYS] jobs=$BOX_LANE_JOBS"
echo "FOREGROUND_START $(date -Is) co-run width $(wc -w <<< "$BOX_LANE_PHYS"), one process per physical core"
[[ "$(sha256sum "$BA" | cut -d' ' -f1)" == "$SA" ]] || { echo LANE_A_SHA_MISMATCH; exit 4; }
[[ "$(sha256sum "$BB" | cut -d' ' -f1)" == "$SB" ]] || { echo LANE_B_SHA_MISMATCH; exit 4; }
read -ra CORES <<< "$BOX_LANE_PHYS"
declare -A busy
n=0
while read -r -u 3 arm lane prob label out; do
  [ -z "${arm:-}" ] && continue
  [[ -f "$out.result" ]] && continue
  bin=$BA; [[ $arm == B ]] && bin=$BB
  mkdir -p "$(dirname "$out")"
  while :; do
    for c in "${CORES[@]}"; do
      pid=${busy[$c]:-}
      if [ -z "$pid" ] || ! kill -0 "$pid" 2>/dev/null; then
        CELL_BIN=$bin PINCPU=$c bash "$L/cell.sh" stat "$lane" "$prob" "$label" 1 "$out" 300 > /dev/null &
        busy[$c]=$!; n=$((n + 1)); continue 3
      fi
    done
    sleep 0.5
  done
done 3< "$JOBS"
wait
echo "cells run: $n $(date -Is)"
echo "FOREGROUND_END $(date -Is)"
