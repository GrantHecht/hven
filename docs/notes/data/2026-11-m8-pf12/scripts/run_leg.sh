#!/usr/bin/env bash
# Four pinned workers on the lane's physical CPUs ($BOX_LANE_PHYS; SMT siblings idle).
S=/home/ghecht/Projects/hven/.claude/worktrees/pf2/docs/notes/data/2026-11-m8-pf12/scripts
read -r a b c d <<< "$BOX_LANE_PHYS"
echo "lane=$BOX_LANE phys=$BOX_LANE_PHYS"
$S/run_worker.sh $a f7_n1000_path_corrupted f7_n800_path_corrupted > $S/../worker1.log 2>&1 &
$S/run_worker.sh $b f7_n1000_path_neutral f7_n2000_path_activity > $S/../worker2.log 2>&1 &
$S/run_worker.sh $c f7_n800_path_neutral f7_n750_path_neutral_control > $S/../worker3.log 2>&1 &
$S/run_worker.sh $d f7_n825_path_neutral_control f7_n2000_path_physics > $S/../worker4.log 2>&1 &
wait
echo LEG_DONE
