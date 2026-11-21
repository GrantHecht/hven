#!/usr/bin/env bash
# The three-seam golden rig (both old-seam adapters), docs/testing.md recipe.  usage: rig3.sh <out-dir>   under flock
set -o pipefail
source "$(dirname "$0")/env.sh"
box_check
OUT=$(realpath -m "${1:?out}"); mkdir -p "$OUT"
D=$WT/build-3seam
rm -rf "$D"
echo "FOREGROUND_START $(date -Is)"
cmake -S "$WT" -B "$D" -G Ninja -DCMAKE_BUILD_TYPE=Release -DHVEN_FP_MODE=SAFER_FAST -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/home/ghecht/Projects/hven/build/_deps/googletest-src \
  -DHVEN_RIG_PSIOPT_SEAM=/home/ghecht/Projects/tycho -DHVEN_RIG_SQP_SEAM=/home/ghecht/Projects/tycho_sqp > "$OUT/configure.txt" 2>&1
rc=$?; echo "configure rc=$rc"; [ $rc -eq 0 ] || { tail -20 "$OUT/configure.txt"; exit $rc; }
grep -iE 'seam|rig' "$OUT/configure.txt" | head -20
ninja -C "$D" -j16 hven_golden_rig > "$OUT/build.txt" 2>&1; rc=$?; echo "build rc=$rc"; tail -2 "$OUT/build.txt"; [ $rc -eq 0 ] || exit $rc
cd "$D" && setsid -w ctest -j4 -R 'golden|Golden|Rig' --output-on-failure > "$OUT/ctest.txt" 2>&1; rc=$?
tail -4 "$OUT/ctest.txt"; echo "ctest rc=$rc"; echo "FOREGROUND_END $(date -Is)"; exit $rc
