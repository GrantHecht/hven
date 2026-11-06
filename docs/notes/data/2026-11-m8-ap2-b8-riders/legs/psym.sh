#!/usr/bin/env bash
# AP-2 B8 riders: P-SYM in two steps (DECLARATION.txt P13). One build directory, build-b8r/psym, rebuilt IN PLACE
# (scripts/psym_compare.sh's precondition). The before arm is build-b8r/psym-before, captured by legs/prebuild.sh
# from a FRESH build-b8r/psym at the merge f0a1c95a BEFORE any rider edit (Release, legs/build.sh's flags,
# CCACHE_DISABLE=1).
#   MID   = the code head + psym/layout_guard.patch (git apply; reverted here with git apply -R, the tracked tree
#           checked clean afterwards): accelerate_zero_tol back under USE_ACCELERATE_SPARSE, i.e. the before
#           arm's layout. Captured to build-b8r/psym-mid.
#   AFTER = the code head. Captured to build-b8r/psym-after.
# STEP 1 psym-before vs psym-mid through the map psym/gen_maps.py generates and psym/exceptions.txt: exit 0.
# STEP 2 psym-mid vs psym-after, no map, no exceptions: psym/step2_check.py prints STEP2_AS_DECLARED.
# usage: psym.sh <evidence-dir>   Run as: flock /tmp/box-build.lock bash psym.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-aad152e17d08967f4
EV=${1:?evidence dir}
P=$WT/docs/notes/data/2026-11-m8-ap2-b8-riders/psym
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$EV"
export CCACHE_DISABLE=1 MKL_NUM_THREADS=1
[ -z "$(git status --porcelain --untracked-files=no)" ] || { echo "TREE_NOT_CLEAN"; exit 1; }
echo "code head $(git rev-parse HEAD)"
( cd "$WT/build-b8r/psym-before" && find . -type f | LC_ALL=C sort | xargs sha256sum ) | sha256sum | sed 's/^/before-capture digest /'
sha256sum "$P/exceptions.txt" "$P/layout_guard.patch"
git apply "$P/layout_guard.patch" || { echo PATCH_FAIL; exit 1; }
ninja -C "$WT/build-b8r/psym" -j16 > "$EV/psym-rebuild-mid.log" 2>&1; r=$?
if [ $r = 0 ]; then rm -rf "$WT/build-b8r/psym-mid"; bash scripts/psym_compare.sh capture "$WT/build-b8r/psym-mid" "$WT/build-b8r/psym" > "$EV/capture-mid.txt" 2>&1 || r=9; fi
git apply -R "$P/layout_guard.patch" || { echo UNPATCH_FAIL; exit 1; }
[ -z "$(git status --porcelain --untracked-files=no)" ] || { echo "TREE_NOT_CLEAN_AFTER_REVERT"; exit 1; }
[ $r = 0 ] || { echo "MID_FAIL rc=$r"; tail -20 "$EV/psym-rebuild-mid.log"; exit 1; }
ninja -C "$WT/build-b8r/psym" -j16 > "$EV/psym-rebuild-after.log" 2>&1 || { echo "REBUILD_FAIL"; tail -20 "$EV/psym-rebuild-after.log"; exit 1; }
rm -rf "$WT/build-b8r/psym-after"
bash scripts/psym_compare.sh capture "$WT/build-b8r/psym-after" "$WT/build-b8r/psym" > "$EV/capture-after.txt" 2>&1 || { echo CAPTURE_FAIL; exit 1; }
python3 "$P/gen_maps.py" "$WT/build-b8r/psym-before" "$WT/build-b8r/psym-mid" "$EV/maps" | tee "$EV/gen-maps.txt"
PSYM_MAX_PAIRS=0 bash scripts/psym_compare.sh compare --symbol-map "$EV/maps/symbol-map.txt" \
  --exceptions "$P/exceptions.txt" "$WT/build-b8r/psym-before" "$WT/build-b8r/psym-mid" > "$EV/compare-step1.txt" 2>&1
rc1=$?
tail -3 "$EV/compare-step1.txt"; echo "step 1 rc=$rc1"
PSYM_MAX_PAIRS=0 bash scripts/psym_compare.sh compare "$WT/build-b8r/psym-mid" "$WT/build-b8r/psym-after" > "$EV/compare-step2.txt" 2>&1
echo "step 2 compare rc=$? (1 expected: the layout step differs)"; tail -2 "$EV/compare-step2.txt"
python3 "$P/step2_check.py" "$EV/compare-step2.txt" "$WT/build-b8r/psym" > "$EV/step2-check.txt" 2>&1; rc2=$?
tail -2 "$EV/step2-check.txt"; echo "step 2 check rc=$rc2"
rc=0; [ $rc1 = 0 ] && [ $rc2 = 0 ] || rc=1
echo "rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
