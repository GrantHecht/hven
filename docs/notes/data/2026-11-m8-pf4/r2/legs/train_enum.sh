#!/usr/bin/env bash
# PF-4 r2 (../../DECLARATION-PF-4-r2.md): copy of ../../legs/train_enum.sh re-pointed to the pf4-r2 worktree and r2/ (paths only); older lines are historical.
# PF-4: ../../2026-11-m8-wa1/legs/train_enum.sh re-pointed (this worktree) with ONE change: cell.sh runs in MODE stat
# (perf stat -x, -e instructions:u,cycles:u around the lane process; the .stat file is the instruction evidence, the row is
# the cell's as in MODE run). TRAIN enumeration (DN-3's 238 roster cells x {default (D), never-default (K)}), serial, one cell
# process at a time, THREADS=1 (core 2), CAP 300 s, through cell.sh. HOLDOUT is never run.
# usage: train_enum.sh <evidence-dir> <lane binary> <lane sha256>
#   Run under (r2): ~/.local/bin/box-lane -- bash train_enum.sh ...
set -uo pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf4-r2
L=$WT/docs/notes/data/2026-11-m8-pf4/r2/legs
EV=${1:?evidence dir}; BIN=${2:?lane binary}; WANT=${3:?lane binary sha256}
ROSTER=$WT/docs/notes/data/2026-11-m8-dn3/roster.tsv
CAP=300
export TMPDIR=$WT/.scratch/tmp
[ -n "${BOX_LANE:-}" ] || { echo "NOT_IN_BOX_LANE (r2: every leg runs inside ~/.local/bin/box-lane)"; exit 3; }; echo "BOX_LANE=$BOX_LANE phys=[$BOX_LANE_PHYS] jobs=$BOX_LANE_JOBS"
echo "FOREGROUND_START $(date -Is)"
[[ "$(sha256sum "$BIN" | cut -d' ' -f1)" == "$WANT" ]] || { echo LANE_BINARY_SHA_MISMATCH; exit 4; }
[[ $(wc -l < "$ROSTER") -eq 238 ]] || { echo ROSTER_COUNT; exit 4; }
grep -qi holdout "$ROSTER" && { echo "HOLDOUT_IN_ROSTER"; exit 3; }
EV=$(realpath -m "$EV"); mkdir -p "$EV/cells"
n=0
while IFS=$'\t' read -r -u 3 pop lane prob; do
  tag="${pop}-${lane}-${prob}"
  for a in D:default K:never-default; do
    arm=${a%%:*}; label=${a#*:}
    [[ -f "$EV/cells/$tag.$arm.result" ]] || CELL_BIN=$BIN bash "$L/cell.sh" stat "$lane" "$prob" "$label" 1 "$EV/cells/$tag.$arm" "$CAP" > /dev/null
  done
  n=$((n + 1))
done 3< "$ROSTER"
echo "cells run: $n $(date -Is)"
echo "FOREGROUND_END $(date -Is)"
