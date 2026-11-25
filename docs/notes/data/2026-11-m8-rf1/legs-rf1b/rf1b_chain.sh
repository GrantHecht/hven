#!/usr/bin/env bash
# RF-1b legs C (ctest rel, dbg), M (control, M20, M21, M22) and the install smoke, in order, one box lane
# (DECLARATION-RF-1b.md). r4_chain.sh re-pointed, M22 added, the install smoke included (RF-1b adds a seam knob and a code path).
# Run under: ~/.local/bin/box-lane -- bash rf1b_chain.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/rf1b
D=$WT/docs/notes/data/2026-11-m8-rf1; L=$D/legs-rf1b; E=$D/gate-RF-1b
echo "FOREGROUND_START $(date -Is) lane=$BOX_LANE"
git -C $WT diff --quiet -- src include tests || { echo TREE_DIRTY; exit 2; }
echo "head $(git -C $WT rev-parse HEAD) code $(git -C $WT log -1 --format=%h -- src include tests)"
mkdir -p $E/suites
bash $L/build.sh $WT Release rel > $E/build-rel.out 2>&1; echo "build rel rc=$?"
bash $L/ctest.sh rel $E/suites/ctest-rel.log | tail -3
bash $L/mutations_rf1b.sh $E/mutation $WT $WT/.scratch/build-rf1/rel M20 M21 M22 > $E/mutations.out 2>&1; echo "mutations rc=$?"; grep -E "^(MUT|VERDICT)" $E/mutations.out
bash $L/build.sh $WT Debug dbg > $E/build-dbg.out 2>&1; echo "build dbg rc=$?"
bash $L/ctest.sh dbg $E/suites/ctest-dbg.log | tail -3
# Y: the production library carries no symbol of the pass_restoration_walk_qps seam (HVEN_TESTING-gated)
{ for a in $(find $WT/.scratch/build-rf1/rel -name 'libhven*.a' | sort); do echo "nm -C $a | grep -c -i pass_restoration_walk_qps:"; nm -C "$a" 2>/dev/null | grep -c -i "pass_restoration_walk_qps"; done; } > $E/symbol-check.txt 2>&1; cat $E/symbol-check.txt
bash $L/install_smoke.sh $E/smoke > $E/smoke.out 2>&1; echo "smoke rc=$?"; tail -4 $E/smoke.out
echo "FOREGROUND_END $(date -Is)"
