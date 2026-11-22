#!/usr/bin/env bash
# RF-1 gate C legs C1 (ctest rel, dbg), C4 (mutations), C5 (install smoke), in order, one box lane.
# Run under: ~/.local/bin/box-lane -- bash c_chain.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/rf1
D=$WT/docs/notes/data/2026-11-m8-rf1; L=$D/legs; E=$D/gate-C
echo "FOREGROUND_START $(date -Is) lane=$BOX_LANE"
git -C $WT diff --quiet -- src include tests || { echo TREE_DIRTY; exit 2; }
echo "head $(git -C $WT rev-parse HEAD) code $(git -C $WT log -1 --format=%h -- src include tests)"
bash $L/build.sh $WT Release rel > $E/build-rel.out 2>&1; echo "build rel rc=$?"
bash $L/ctest.sh rel $E/suites/ctest-rel.log | tail -3
bash $L/mutations.sh $E/mutation $WT $WT/.scratch/build-rf1/rel C1 C2 C3 C4 C5 C6 C7 C8 C9 C10 C11 C12 R1 > $E/mutations.out 2>&1; echo "mutations rc=$?"; grep -E "^(MUT|VERDICT)" $E/mutations.out
bash $L/build.sh $WT Debug dbg > $E/build-dbg.out 2>&1; echo "build dbg rc=$?"
bash $L/ctest.sh dbg $E/suites/ctest-dbg.log | tail -3
bash $L/install_smoke.sh $E/install-smoke > $E/install-smoke.out 2>&1; echo "smoke rc=$?"; grep -E "rc=" $E/install-smoke.out
echo "FOREGROUND_END $(date -Is)"
