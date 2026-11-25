#!/usr/bin/env bash
# RF-1c legs C (ctest rel, dbg), M (control, M1, M2, R1), N (counter replay vs origin/m8) and the install smoke, in order, one box lane
# (DECLARATION-RF-1c.md). rf1b_chain.sh re-pointed; the replay leg is new to the chain (replay.sh), the seam-symbol check is dropped (RF-1c adds
# no seam knob). usage: rf1c_chain.sh <base-src: a worktree of origin/m8 976739bc6 with submodules>
# Run under: ~/.local/bin/box-lane -- bash rf1c_chain.sh <base-src>
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/rf1c
BASESRC=${1:?base source tree}
B=/home/ghecht/Projects/hven/.scratch/rf1c-build-legs/build-rf1
D=$WT/docs/notes/data/2026-11-m8-rf1; L=$D/legs-rf1c; E=$D/gate-RF-1c
echo "FOREGROUND_START $(date -Is) lane=$BOX_LANE"
git -C $WT diff --quiet -- src include tests || { echo TREE_DIRTY; exit 2; }
[ "$(git -C $BASESRC rev-parse HEAD)" = 976739bc6f10a314a219fb35ec7c656c5aff8f76 ] || { echo BASE_SRC_WRONG; exit 2; }
echo "head $(git -C $WT rev-parse HEAD) code $(git -C $WT log -1 --format=%h -- src include tests)"
mkdir -p $E/suites $E/replay
bash $L/build.sh $WT Release rel > $E/build-rel.out 2>&1; echo "build rel rc=$?"
bash $L/ctest.sh rel $E/suites/ctest-rel.log | tail -3
bash $L/build.sh $BASESRC Release base hven_sqp_corpus > $E/build-base.out 2>&1; echo "build base corpus rc=$?"
sha256sum $B/rel/bench/hven_sqp_corpus $B/base/bench/hven_sqp_corpus
bash $L/replay.sh $WT $B/base/bench/hven_sqp_corpus $B/rel/bench/hven_sqp_corpus $E/replay > $E/replay.out 2>&1; echo "replay rc=$?"; grep -E "^(worst|base|head|compare)|Replay|mismatch|MISMATCH" $E/replay.out | head -30
bash $L/mutations_rf1c.sh $E/mutation $WT $B/rel M1 M2 R1 > $E/mutations.out 2>&1; echo "mutations rc=$?"; grep -E "^(MUT|VERDICT)" $E/mutations.out
bash $L/build.sh $WT Debug dbg > $E/build-dbg.out 2>&1; echo "build dbg rc=$?"
bash $L/ctest.sh dbg $E/suites/ctest-dbg.log | tail -3
bash $L/install_smoke.sh $E/smoke > $E/smoke.out 2>&1; echo "smoke rc=$?"; tail -4 $E/smoke.out
echo "FOREGROUND_END $(date -Is)"
