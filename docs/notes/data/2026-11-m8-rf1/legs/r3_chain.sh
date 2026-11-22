#!/usr/bin/env bash
# RF-1 gate A+B2 r3 legs C (ctest rel, dbg), M (mutations M1-M20, R1), S (install smoke), Y (production-library symbol check), in order, one box lane.
# Derived from ab2_chain.sh. Run under: ~/.local/bin/box-lane -- bash r3_chain.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/rf1
D=$WT/docs/notes/data/2026-11-m8-rf1; L=$D/legs; E=$D/gate-AB2-r3
echo "FOREGROUND_START $(date -Is) lane=$BOX_LANE"
git -C $WT diff --quiet -- src include tests || { echo TREE_DIRTY; exit 2; }
echo "head $(git -C $WT rev-parse HEAD) code $(git -C $WT log -1 --format=%h -- src include tests)"
mkdir -p $E/suites
bash $L/build.sh $WT Release rel > $E/build-rel.out 2>&1; echo "build rel rc=$?"
bash $L/ctest.sh rel $E/suites/ctest-rel.log | tail -3
# Y: the production library carries no symbol of the restoration_capture_limit seam (HVEN_TESTING-gated)
{ echo "libhven.a: $(find $WT/.scratch/build-rf1/rel -name libhven.a | head -1)"
  for a in $(find $WT/.scratch/build-rf1/rel -name libhven.a); do
    echo "nm -C $a | grep -c -i capture_limit:"; nm -C "$a" 2>/dev/null | grep -c -i "capture_limit"; done
  echo "strings(count) restoration_capture_limit in libhven.a:"; strings -a $(find $WT/.scratch/build-rf1/rel -name libhven.a | head -1) | grep -c "restoration_capture_limit"
} > $E/symbol-check.txt 2>&1; cat $E/symbol-check.txt
bash $L/mutations_ab2_r3.sh $E/mutation $WT $WT/.scratch/build-rf1/rel M1 M2 M3 M4 M5 M6 M7 M8 M9 M10 M11 M12 M13 M14 M15 M16 M17 M18 M19 M20 R1 > $E/mutations.out 2>&1; echo "mutations rc=$?"; grep -E "^(MUT|VERDICT)" $E/mutations.out
bash $L/build.sh $WT Debug dbg > $E/build-dbg.out 2>&1; echo "build dbg rc=$?"
bash $L/ctest.sh dbg $E/suites/ctest-dbg.log | tail -3
bash $L/install_smoke.sh $E/install-smoke > $E/install-smoke.out 2>&1; echo "smoke rc=$?"; grep -E "rc=" $E/install-smoke.out
echo "FOREGROUND_END $(date -Is)"
