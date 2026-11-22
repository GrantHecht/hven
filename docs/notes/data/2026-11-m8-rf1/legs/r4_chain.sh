#!/usr/bin/env bash
# RF-1 gate A+B2 r4 legs C (ctest rel, dbg) and M (control, M19, M20, M21), in order, one box lane (DECLARATION-RF-1-AB2-r4.md).
# r3_chain.sh without the smoke and symbol legs (F-2 adds no seam and no option), the mutation table mutate_ab2_r4.py.
# Run under: ~/.local/bin/box-lane -- bash r4_chain.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/rf1
D=$WT/docs/notes/data/2026-11-m8-rf1; L=$D/legs; E=$D/gate-AB2-r4
echo "FOREGROUND_START $(date -Is) lane=$BOX_LANE"
git -C $WT diff --quiet -- src include tests || { echo TREE_DIRTY; exit 2; }
echo "head $(git -C $WT rev-parse HEAD) code $(git -C $WT log -1 --format=%h -- src include tests)"
mkdir -p $E/suites
bash $L/build.sh $WT Release rel > $E/build-rel.out 2>&1; echo "build rel rc=$?"
bash $L/ctest.sh rel $E/suites/ctest-rel.log | tail -3
bash $L/mutations_ab2_r4.sh $E/mutation $WT $WT/.scratch/build-rf1/rel M19 M20 M21 > $E/mutations.out 2>&1; echo "mutations rc=$?"; grep -E "^(MUT|VERDICT)" $E/mutations.out
bash $L/build.sh $WT Debug dbg > $E/build-dbg.out 2>&1; echo "build dbg rc=$?"
bash $L/ctest.sh dbg $E/suites/ctest-dbg.log | tail -3
echo "FOREGROUND_END $(date -Is)"
