#!/usr/bin/env bash
# Not a leg. One lane binary per arm: a branch of the local lane clone .scratch/hb (a clone of hven-bench-dn1, never pushed)
# taken from dense-gate-legs-r2 (the latest TRAIN lane: DN-3's flip lane + the pin), with ONLY the pin moved to the arm's
# commit; scripts/build_hven_pin.sh (.hven/install-<sha12>), configure, build hven_bench_cell, frozen to build-zs/lane<ARM>/.
# usage: lane.sh <A|B> <full pin sha>   Run as: flock /tmp/box-build.lock bash lane.sh ...
set -o pipefail
. "$(dirname "$0")/../legs/env.sh"
ARM=${1:?A|B}; PIN=${2:?full pin sha}
LANE_FROM=dense-gate-legs-r2
pgrep_guard
echo "FOREGROUND_START $(date -Is)"
[ "$(git -C "$WT" rev-parse "$PIN^{commit}")" = "$PIN" ] || { echo PIN_NOT_FULL; exit 4; }
if [ ! -d "$HB/.git" ]; then
  git clone --quiet /home/ghecht/Projects/hven-bench-dn1 "$HB" || exit 1
  git -C "$HB" checkout --quiet -b zs-lane-base "origin/$LANE_FROM" || exit 1
  mkdir -p "$HB/.scratch" && cp -a /home/ghecht/Projects/hven-bench-dn1/.scratch/h3 "$HB/.scratch/h3" || exit 1
fi
cd "$HB" || exit 1
br=zs-lane-$ARM
git rev-parse --verify -q "$br" > /dev/null && { echo "BRANCH_EXISTS $br"; exit 4; }
git checkout --quiet -b "$br" "origin/$LANE_FROM" || exit 1
sed -i -e "s/^set(HVEN_PIN_SHA \".*\")$/set(HVEN_PIN_SHA \"$PIN\")/" \
       -e "s/^set(HVEN_PIN_BRANCH \".*\")$/set(HVEN_PIN_BRANCH \"zero-step-$ARM\")/" cmake/hven_pin.cmake
git diff --stat
git -c user.name="Grant Hecht" -c user.email=grantrhecht@icloud.com commit --quiet -am "EVIDENCE-ONLY (local clone, never pushed): pin hven ${PIN:0:9} (zero-step arm $ARM)" || exit 1
bash scripts/build_hven_pin.sh > "$B/lane$ARM-pin.log" 2>&1 || { echo PIN_BUILD_FAIL; tail -20 "$B/lane$ARM-pin.log"; exit 1; }
prefix="$HB/.hven/install-${PIN:0:12}"
bdir="$HB/.scratch/$br-build"
rm -rf "$bdir"
cmake -S "$HB" -B "$bdir" -G Ninja -Uhven_DIR -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_PREFIX_PATH="$prefix" > "$B/lane$ARM-configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -20 "$B/lane$ARM-configure.log"; exit 1; }
cmake --build "$bdir" --parallel 12 --target hven_bench_cell > "$B/lane$ARM-build.log" 2>&1 || { echo BUILD_FAIL; tail -20 "$B/lane$ARM-build.log"; exit 1; }
mkdir -p "$B/lane$ARM"
cp -p "$bdir/lanes/hven/hven_bench_cell" "$B/lane$ARM/hven_bench_cell" || exit 1
grep -E 'HVEN_PIN_SHA' "$prefix/HVEN_PIN.txt"
sha256sum "$B/lane$ARM/hven_bench_cell" "$prefix/lib64/libhven.a"
git log --oneline -1
echo "FOREGROUND_END $(date -Is)"
