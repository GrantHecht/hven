#!/usr/bin/env bash
# usage: train.sh <lane binary> <out csv>   TRAIN roster (238 cells: HS 44 + small-dense 194), SERIAL (one cpu), MKL_NUM_THREADS=1; instructions:u + the cell rows.
# Run under `box-lane --`.  HOLDOUT is never run (the roster is the DN-3 TRAIN roster).
W=/home/ghecht/Projects/hven/.claude/worktrees/pf7; D=$W/docs/notes/data/2026-11-m8-pf7
BIN=${1:?bin}; OUT=$(realpath -m "${2:?out}")
cpu=$(echo $BOX_LANE_PHYS | awk '{print $1}')
cd /home/ghecht/Projects/hven-bench-pf7 || exit 1
sha256sum "$BIN"
python3 $D/tools/icount.py "$BIN" $D/cells-train238.csv "$OUT" "$cpu" 300
