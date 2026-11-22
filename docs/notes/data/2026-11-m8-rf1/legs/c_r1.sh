#!/usr/bin/env bash
# RF-1 gate C-r1 leg C1: ctest rel and dbg at the head. Run under: ~/.local/bin/box-lane -- bash c_r1.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/rf1
D=$WT/docs/notes/data/2026-11-m8-rf1; L=$D/legs; E=$D/gate-C-r1
echo "FOREGROUND_START $(date -Is) lane=$BOX_LANE head $(git -C $WT rev-parse HEAD) code $(git -C $WT log -1 --format=%h -- src include tests)"
git -C $WT diff --quiet -- src include tests || { echo TREE_DIRTY; exit 2; }
for T in rel:Release dbg:Debug; do d=${T%%:*}; bash $L/build.sh $WT ${T#*:} $d > $E/build-$d.out 2>&1; echo "build $d rc=$?"; bash $L/ctest.sh $d $E/ctest-$d.log | grep -E "rc=|tests passed"; grep "tests passed" $E/ctest-$d.log; done
echo "FOREGROUND_END $(date -Is)"
