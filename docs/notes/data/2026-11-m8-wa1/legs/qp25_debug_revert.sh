#!/usr/bin/env bash
# WA-1 r7 qp25 close note evidence (not a leg): qp25 in Debug with and without the footprint rung revert (mutation N15 of
# mutate_r7.py), build-wa/dbg, hven_qp_seam_tests. usage: qp25_debug_revert.sh <out-dir>   Run under flock /tmp/box-build.lock setsid -w
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/wa1; L=$WT/docs/notes/data/2026-11-m8-wa1/legs; OUT=${1:?}
export MKL_NUM_THREADS=1 TMPDIR=$WT/build-wa/tmp
cd "$WT" || exit 1
git diff --quiet -- src include || { echo TREE_DIRTY; exit 2; }
mkdir -p "$OUT"
F='QpYrkRefusedExchange.YorknetRestorationQp25*:QpWa1AscentFootprint.SyntheticRungOnASuspectFaceIsReverted'
for arm in code N15; do
  [ $arm = code ] || python3 "$L/mutate_r7.py" N15 || exit 1
  ninja -C build-wa/dbg hven_qp_seam_tests > "$OUT/$arm.build.log" 2>&1 || { echo BUILD_FAIL; python3 "$L/mutate_r7.py" restore; exit 1; }
  build-wa/dbg/tests/qp/hven_qp_seam_tests --gtest_filter="$F" > "$OUT/$arm.log" 2>&1; echo "$arm rc=$?"
  grep -E "^\[  (FAILED|PASSED|  OK) " "$OUT/$arm.log"
  [ $arm = code ] || python3 "$L/mutate_r7.py" restore > /dev/null
done
ninja -C build-wa/dbg hven_qp_seam_tests > "$OUT/final.build.log" 2>&1
git diff --quiet -- src include && echo TREE_RESTORED
