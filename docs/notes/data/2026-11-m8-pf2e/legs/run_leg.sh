#!/usr/bin/env bash
# PF-2e P3 instruction leg: four pinned workers on the lane's physical CPUs ($BOX_LANE_PHYS; SMT siblings idle).
# Run under: ~/.local/bin/box-lane -- bash run_leg.sh
S=/home/ghecht/Projects/hven/.claude/worktrees/pf2e/docs/notes/data/2026-11-m8-pf2e
sha() { grep -F "$1" $S/BINARIES.txt | awk '{print $NF}'; }
[ "$(sha256sum /home/ghecht/Projects/hven/.claude/worktrees/pf2d/.scratch/build-measL/bench/hven_sqp_corpus | cut -d' ' -f1)" = "$(sha 'measL')" ] || { echo SHA_MISMATCH_measL; exit 4; }
[ "$(sha256sum /home/ghecht/Projects/hven/.claude/worktrees/pf2e/.scratch/build-pf2e/measE/bench/hven_sqp_corpus | cut -d' ' -f1)" = "$(sha 'measE')" ] || { echo SHA_MISMATCH_measE; exit 4; }
mkdir -p $S/measure/raw $S/measure/perf
read -r a b c d <<< "$BOX_LANE_PHYS"
echo "lane=$BOX_LANE phys=$BOX_LANE_PHYS $(date -Is)"
$S/legs/run_worker.sh $a f7_n1000_path_corrupted f7_n800_path_corrupted > $S/measure/worker1.log 2>&1 &
$S/legs/run_worker.sh $b f7_n1000_path_neutral f7_n2000_path_activity > $S/measure/worker2.log 2>&1 &
$S/legs/run_worker.sh $c f7_n800_path_neutral f7_n750_path_neutral_control > $S/measure/worker3.log 2>&1 &
$S/legs/run_worker.sh $d f7_n825_path_neutral_control f7_n2000_path_physics > $S/measure/worker4.log 2>&1 &
wait
echo "LEG_DONE $(date -Is)"
