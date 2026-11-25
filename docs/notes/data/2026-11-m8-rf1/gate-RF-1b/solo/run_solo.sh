#!/usr/bin/env bash
# RF-1b: the one TRAIN mover (MESH.K) re-run ALONE, serial, one pinned core, both lane binaries, twice each (CLAUDE.md section 7:
# a deviation under a co-run is a candidate until reproduced alone). Run under: ~/.local/bin/box-lane -- bash run_solo.sh
WT=/home/ghecht/Projects/hven/.claude/worktrees/rf1b
D=$WT/docs/notes/data/2026-11-m8-rf1; EV=$D/gate-RF-1b/solo
read -r p1 _ <<< "$BOX_LANE_PHYS"
for rep in 1 2; do for arm in R H; do
  PIN_CORE=$p1 CELL_BIN=$WT/.scratch/bin/$arm/hven_bench_cell bash $D/legs-rf1b/cell.sh run hven-sqp MESH never-default 1 $EV/MESH.K.$arm.$rep 300
done; done
