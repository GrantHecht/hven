#!/usr/bin/env bash
# RF-1 gate A+B2 legs C (ctest rel, dbg), M (mutations), S (install smoke), in order, one box lane.
# Run under: ~/.local/bin/box-lane -- bash ab2_chain.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/rf1
D=$WT/docs/notes/data/2026-11-m8-rf1; L=$D/legs; E=$D/gate-AB2
echo "FOREGROUND_START $(date -Is) lane=$BOX_LANE"
git -C $WT diff --quiet -- src include tests || { echo TREE_DIRTY; exit 2; }
echo "head $(git -C $WT rev-parse HEAD) code $(git -C $WT log -1 --format=%h -- src include tests)"
mkdir -p $E/suites
bash $L/build.sh $WT Release rel > $E/build-rel.out 2>&1; echo "build rel rc=$?"
bash $L/ctest.sh rel $E/suites/ctest-rel.log | tail -3
bash $L/mutations_ab2.sh $E/mutation $WT $WT/.scratch/build-rf1/rel M1 M2 M3 M4 M5 M6 M7 M8 M9 M10 M11 M12 M13 M14 M15 M16 M17 M18 M19 R1 > $E/mutations.out 2>&1; echo "mutations rc=$?"; grep -E "^(MUT|VERDICT)" $E/mutations.out
bash $L/build.sh $WT Debug dbg > $E/build-dbg.out 2>&1; echo "build dbg rc=$?"
bash $L/ctest.sh dbg $E/suites/ctest-dbg.log | tail -3
bash $L/install_smoke.sh $E/install-smoke > $E/install-smoke.out 2>&1; echo "smoke rc=$?"; grep -E "rc=" $E/install-smoke.out
echo "FOREGROUND_END $(date -Is)"
