#!/usr/bin/env bash
# RF-1 gate A+B2 leg P (the TU-1 pilot's failing cells): a private copy of the DIAGQP harness copy
# (/home/ghecht/Projects/hven/.scratch/tu1-diag/hb, the TU-1 harness at 146e6ec with the DIAGQP stderr line; it reads
# `base+key=value` option labels) at .scratch/hbd, pinned to <sha>, one lane binary per tag into .scratch/bin/pilot-<tag>/.
# The hven-bench-tu1 tree is never touched. usage: pilot_build.sh <tag> <hven sha (40 hex)>   Run under box-lane.
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/rf1
SRC=/home/ghecht/Projects/hven/.scratch/tu1-diag/hb
HB=$WT/.scratch/hbd
TAG=${1:?tag}; PIN_SHA=${2:?sha}
out=$WT/.scratch/bin/pilot-$TAG
export MKL_NUM_THREADS=1 TMPDIR=$WT/.scratch/tmp
mkdir -p "$TMPDIR" "$out"
[ "$(git -C "$WT" rev-parse "$PIN_SHA^{commit}")" = "$PIN_SHA" ] || { echo PIN_SHA_NOT_FULL; exit 4; }
[ -d "$HB" ] || { mkdir -p "$HB" && (cd "$SRC" && tar --exclude=./.hven --exclude=./.scratch -cf - .) | (cd "$HB" && tar -xf -); } || exit 1
sed -i -e "s/^set(HVEN_PIN_SHA \".*\")$/set(HVEN_PIN_SHA \"$PIN_SHA\")/" \
       -e "s/^set(HVEN_PIN_BRANCH \".*\")$/set(HVEN_PIN_BRANCH \"rf1-$TAG\")/" "$HB/cmake/hven_pin.cmake"
(cd "$HB" && bash scripts/build_hven_pin.sh) > "$out/pin.log" 2>&1 || { echo PIN_BUILD_FAIL; tail -20 "$out/pin.log"; exit 1; }
prefix="$HB/.hven/install-${PIN_SHA:0:12}"
bdir="$WT/.scratch/hbd-build-$TAG"
rm -rf "$bdir"
cmake -S "$HB" -B "$bdir" -G Ninja -Uhven_DIR -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_PREFIX_PATH="$prefix" > "$out/configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -20 "$out/configure.log"; exit 1; }
cmake --build "$bdir" --parallel ${BOX_LANE_JOBS:-4} --target hven_bench_cell > "$out/build.log" 2>&1 || { echo BUILD_FAIL; tail -20 "$out/build.log"; exit 1; }
cp -p "$bdir/lanes/hven/hven_bench_cell" "$out/hven_bench_cell" || exit 1
grep -E 'HVEN_PIN_SHA' "$prefix/HVEN_PIN.txt"
sha256sum "$out/hven_bench_cell"
