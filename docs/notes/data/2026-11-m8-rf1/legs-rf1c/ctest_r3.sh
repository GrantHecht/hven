#!/usr/bin/env bash
# RF-1c r3 leg C: build and ctest Release and Debug at the committed head (the riders). Run under: ~/.local/bin/box-lane -- bash ctest_r3.sh
WT=/home/ghecht/Projects/hven/.claude/worktrees/rf1c
L=$WT/docs/notes/data/2026-11-m8-rf1/legs-rf1c; E=$WT/docs/notes/data/2026-11-m8-rf1/gate-RF-1c/suites-r3
echo "FOREGROUND_START $(date -Is) head $(git -C $WT rev-parse HEAD)"
git -C $WT diff --quiet -- src include tests || { echo TREE_DIRTY; exit 2; }
for t in "Release rel" "Debug dbg"; do set -- $t
  bash $L/build.sh $WT $1 $2 > $E/build-$2.out 2>&1; echo "build $2 rc=$?"; cat $E/build-$2.out | tail -2
  bash $L/ctest.sh $2 $E/ctest-$2.log | tail -4
done
echo "FOREGROUND_END $(date -Is)"
