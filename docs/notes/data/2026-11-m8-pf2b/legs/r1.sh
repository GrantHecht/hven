#!/usr/bin/env bash
# PF-2b r1 legs: P1 (rel, dbg) and P4 restricted to control + M3. Run under: ~/.local/bin/box-lane -- bash r1.sh
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf2b; D=$WT/docs/notes/data/2026-11-m8-pf2b; L=$D/legs
mkdir -p $D/gate-r1/suites $D/gate-r1/mutation
for t in "Release rel" "Debug dbg"; do set -- $t; bash $L/build.sh $WT $1 $2 && bash $L/ctest.sh $2 $D/gate-r1/suites/ctest-$2.log; grep "tests passed" $D/gate-r1/suites/ctest-$2.log; done
bash $L/mutations.sh $D/gate-r1/mutation M3
echo R1_DONE
