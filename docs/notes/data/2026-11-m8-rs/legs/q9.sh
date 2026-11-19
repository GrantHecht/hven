#!/usr/bin/env bash
# usage: WT=<worktree> flock /tmp/box-build.lock setsid -w bash q9.sh <arm-name> <out-dir> <seam-target>...
# Q9 (DECLARATION-RS6a.md): a clean configure of the commit checked out in $WT into $WT/build-q9, the library and gtest prebuilt (ninja hven gtest gtest_main), then `ninja -j16` of exactly the seam
# targets alone, measured by tools/q9_measure.py; then the rest of the tree (`ninja -j16`) measured the same way. Run once per arm, each in the worktree holding that arm's commit.
set -o pipefail
source "$(dirname "$0")/env.sh"
box_check
ARM=${1:?arm}; OUT=$(realpath -m "${2:?out}"); shift 2
E=$WT/docs/notes/data/2026-11-m8-rs
D=$WT/build-q9
mkdir -p "$OUT"; rm -rf "$D"
echo "FOREGROUND_START $(date -Is) arm=$ARM head=$(git -C "$WT" rev-parse --short HEAD)"
GIT_DIR=$(git -C "$WT" rev-parse --absolute-git-dir) GIT_WORK_TREE=$WT \
cmake -S "$WT" -B "$D" -G Ninja -DCMAKE_BUILD_TYPE=Release -DHVEN_FP_MODE=SAFER_FAST -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_EXAMPLES=ON \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/home/ghecht/Projects/hven/build/_deps/googletest-src > "$OUT/$ARM-configure.log" 2>&1 || { echo CONFIGURE_FAIL; exit 1; }
ninja -C "$D" -j16 hven gtest gtest_main > "$OUT/$ARM-prebuild.log" 2>&1 || { echo PREBUILD_FAIL; tail "$OUT/$ARM-prebuild.log"; exit 1; }
sync; sleep 5
python3 "$E/tools/q9_measure.py" "$OUT/$ARM-seams.json" -- ninja -C "$D" -j16 "$@"; rc1=$?
python3 "$E/tools/q9_measure.py" "$OUT/$ARM-rest.json" -- ninja -C "$D" -j16; rc2=$?
echo "seams rc=$rc1 rest rc=$rc2"; echo "FOREGROUND_END $(date -Is)"
rm -rf "$D"
exit $((rc1 + rc2))
