#!/usr/bin/env bash
# inside a box-lane lane: the C4 instruction leg. width = $BOX_LANE_JOBS, cpus = $BOX_LANE_PHYS
set -euo pipefail
E=/home/ghecht/Projects/hven/.claude/worktrees/pf6/docs/notes/data/2026-11-m8-pf6
export TMPDIR=/home/ghecht/Projects/hven/.claude/worktrees/pf6/.scratch/tmp; mkdir -p $TMPDIR
CPUS=$(echo $BOX_LANE_PHYS | tr ' ' ',')
echo "lane=$BOX_LANE phys=$BOX_LANE_PHYS date=$(date -Is)"
sha256sum /home/ghecht/Projects/hven-bench-pf6-*/build/lanes/hven/hven_bench_cell
python3 $E/tools/pf6_stat.py $E/../2026-11-m8-pf3/cells.csv $E/out/stat 4 $CPUS 2 intel:/home/ghecht/Projects/hven-bench-pf6-intel sequential:/home/ghecht/Projects/hven-bench-pf6-sequential
