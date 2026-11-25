#!/usr/bin/env bash
# RF-1c leg U (the TU-2 cutest cells): a private copy of the TU-2 harness (hven-bench-tu2 HEAD c3d176e, `git archive`: tracked files only, the
# tree itself untouched; sets/cutest/sif and .scratch/h3/decoded copied from it) at rf1c-build-legs/hbt, pinned to <sha>, one lane binary per tag
# into rf1c-build-legs/bin/tu2-<tag>/. The hven install at the pin is the TRAIN lane's, linked by symlink (lane_build.sh runs first).
# Unlike the TRAIN lanes, this binary is NOT the NEVER=1 patch: it is the harness as TU-2 ran it (labels `default` / `sqp-qn` plus `+key=value`).
# usage: tu2_build.sh <tag> <hven sha (40 hex)>   Run under box-lane.
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/rf1c
R=/home/ghecht/Projects/hven/.scratch/rf1c-build-legs
SRC=/home/ghecht/Projects/hven-bench-tu2
HB=$R/hbt
TAG=${1:?tag}; PIN_SHA=${2:?sha}
out=$R/bin/tu2-$TAG
export MKL_NUM_THREADS=1 TMPDIR=$R/tmp
mkdir -p "$TMPDIR" "$out"
[ "$(git -C "$WT" rev-parse "$PIN_SHA^{commit}")" = "$PIN_SHA" ] || { echo PIN_SHA_NOT_FULL; exit 4; }
if [ ! -d "$HB" ]; then
  mkdir -p "$HB" && git -C "$SRC" archive c3d176e | tar -xf - -C "$HB" || exit 1
  mkdir -p "$HB/sets/cutest" "$HB/.scratch/h3"
  cp -r "$SRC/sets/cutest/sif" "$HB/sets/cutest/sif" && cp -r "$SRC/.scratch/h3/decoded" "$HB/.scratch/h3/decoded" || exit 1
fi
sed -i -e "s/^set(HVEN_PIN_SHA \".*\")$/set(HVEN_PIN_SHA \"$PIN_SHA\")/" \
       -e "s/^set(HVEN_PIN_BRANCH \".*\")$/set(HVEN_PIN_BRANCH \"rf1c-$TAG\")/" "$HB/cmake/hven_pin.cmake"
TRAINPFX=$R/hb/.hven/install-${PIN_SHA:0:12}
[ -f "$TRAINPFX/HVEN_PIN.txt" ] || { echo "NO_TRAIN_INSTALL $TRAINPFX"; exit 1; }
mkdir -p "$HB/.hven" && ln -sfn "$TRAINPFX" "$HB/.hven/install-${PIN_SHA:0:12}" || exit 1
prefix="$HB/.hven/install-${PIN_SHA:0:12}"
bdir="$R/hbt-build-$TAG"
rm -rf "$bdir"
cmake -S "$HB" -B "$bdir" -G Ninja -Uhven_DIR -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_PREFIX_PATH="$prefix" -DHVEN_BENCH_IPOPT_ROOT=/nonexistent-rf1c-ipopt-off -DHVEN_BENCH_SNOPT_ROOT=/nonexistent-rf1c-snopt-off \
  -DHVEN_BENCH_CUTEST_ROOT=/home/ghecht/Software/cutest > "$out/configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -20 "$out/configure.log"; exit 1; }
cmake --build "$bdir" --parallel ${BOX_LANE_JOBS:-4} --target hven_bench_cell > "$out/build.log" 2>&1 || { echo BUILD_FAIL; tail -20 "$out/build.log"; exit 1; }
cp -p "$bdir/lanes/hven/hven_bench_cell" "$out/hven_bench_cell" || exit 1
grep -E 'HVEN_PIN_SHA' "$prefix/HVEN_PIN.txt"
sha256sum "$out/hven_bench_cell"
