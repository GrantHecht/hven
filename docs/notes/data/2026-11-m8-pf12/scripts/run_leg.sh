#!/usr/bin/env bash
# Four pinned workers (physical cores 4-7; SMT siblings 12-15 left unused by this leg).
S=/home/ghecht/Projects/hven/.claude/worktrees/pf2/docs/notes/data/2026-11-m8-pf12/scripts
$S/run_worker.sh 4 f7_n1000_path_corrupted f7_n800_path_corrupted > $S/../worker4.log 2>&1 &
$S/run_worker.sh 5 f7_n1000_path_neutral f7_n2000_path_activity > $S/../worker5.log 2>&1 &
$S/run_worker.sh 6 f7_n800_path_neutral f7_n750_path_neutral_control > $S/../worker6.log 2>&1 &
$S/run_worker.sh 7 f7_n825_path_neutral_control f7_n2000_path_physics > $S/../worker7.log 2>&1 &
wait
echo LEG_DONE
