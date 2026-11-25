#!/usr/bin/env bash
# Leg C (DECLARATION-QPC section 6): FULL ctest Release and Debug at H (I19-2a legC.sh, unchanged in substance). Run under: setsid ~/.local/bin/box-lane -- bash legC.sh <worktree> <outdir>
set -o pipefail
WT=$(realpath "$1"); OUT=$(realpath -m "$2"); mkdir -p "$OUT"
export CCACHE_DISABLE=1
echo "LEG C start $(date -Is) head=$(git -C $WT rev-parse HEAD) lane=$BOX_LANE jobs=${BOX_LANE_JOBS:-4}"
for t in "Release build-rel rel" "Debug build-dbg dbg"; do set -- $t
  D=$WT/.scratch/$2
  if [ ! -f "$D/build.ninja" ]; then
    cmake -S "$WT" -B "$D" -G Ninja -DCMAKE_BUILD_TYPE=$1 -DHVEN_FP_MODE=SAFER_FAST -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
      -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_EXAMPLES=ON > "$OUT/configure-$3.log" 2>&1 || { echo "CONFIGURE_FAIL $1"; exit 1; }
  fi
  echo "== $1 build start $(date -Is)"
  ninja -C "$D" -j${BOX_LANE_JOBS:-4} > "$OUT/build-$3.log" 2>&1; rc=$?
  echo "== $1 build rc=$rc end $(date -Is)"; [ $rc -eq 0 ] || exit 1
  (cd "$D" && ctest -j${BOX_LANE_JOBS:-4} > "$OUT/ctest-$3.log" 2>&1); echo "== $1 ctest rc=$? end $(date -Is)"
  tail -6 "$OUT/ctest-$3.log"
done
echo "LEG C end $(date -Is)"
