#!/usr/bin/env bash
# TU-0 TRAIN lane setup (not a leg): I17's train_setup.sh on hven-bench main e03cbad with hb_patch_never.py (the never- label prefix); the RS include fix is already in that main.
# A PRIVATE clone (<worktree>/.scratch/hb, git-ignored, never pushed) of the local hven-bench at main e03cbad + the label patch + the RS include fix, pin -> <sha>, one lane binary frozen into
# <worktree>/.scratch/bin/<ARM>/hven_bench_cell.   ARM A = the declared base, ARM B = the TU-0 code.
# usage: train_setup.sh <worktree> <A|B> <hven sha (40 hex)> [round tag]   Run as: flock /tmp/box-build.lock setsid -w bash train_setup.sh ...
set -o pipefail
WT=${1:?worktree}; ARM=${2:?A|B}; PIN_SHA=${3:?sha}; ROUND=${4:-}
HB=$WT/.scratch/hb
SRC=/home/ghecht/Projects/hven-bench
L=$(cd "$(dirname "$0")" && pwd)
OUT=$WT/.scratch/bin/$ARM
export MKL_NUM_THREADS=1 TMPDIR=$WT/.scratch/tmp GIT_CEILING_DIRECTORIES=$WT/.scratch HVEN_BENCH_HVEN_REPO=/home/ghecht/Projects/hven
mkdir -p "$TMPDIR" "$OUT"
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo PGREP_BUSY; exit 3; fi
echo PGREP_EMPTY; echo "FOREGROUND_START $(date -Is)"
[ "$(git -C "$WT" rev-parse "$PIN_SHA^{commit}")" = "$PIN_SHA" ] || { echo PIN_SHA_NOT_FULL; exit 4; }
if [ ! -d "$HB/.git" ]; then
  git clone --quiet "$SRC" "$HB" || exit 1
  git -C "$HB" checkout --quiet -b "tu0-base" e03cbad456d0c93fa543f5575bead45977101277 || exit 1
fi
[ -d "$HB/sets/cutest" ] || cp -r "$SRC/sets/cutest" "$HB/sets/cutest" || exit 1
mkdir -p "$HB/.scratch/h3" && [ -d "$HB/.scratch/h3/decoded" ] || cp -r /home/ghecht/Projects/hven-bench-dn1/.scratch/h3/decoded "$HB/.scratch/h3/decoded" || exit 1
cd "$HB" || exit 1
br=tu0$ROUND-$ARM
git rev-parse --verify -q "$br" > /dev/null && { echo "BRANCH_EXISTS $br"; exit 4; }
git checkout --quiet -b "$br" e03cbad456d0c93fa543f5575bead45977101277 || exit 1
python3 "$L/hb_patch_never.py" lanes/hven/hven_bench_cell.cpp || exit 1
sed -i -e "s/^set(HVEN_PIN_SHA \".*\")$/set(HVEN_PIN_SHA \"$PIN_SHA\")/" \
       -e "s/^set(HVEN_PIN_BRANCH \".*\")$/set(HVEN_PIN_BRANCH \"tu0-$ARM\")/" cmake/hven_pin.cmake
git diff --stat
git -c user.name="Grant Hecht" -c user.email=grantrhecht@icloud.com commit --quiet -am "EVIDENCE-ONLY (private clone, never pushed): pin hven ${PIN_SHA:0:9} (tu0 arm $ARM)" || exit 1
bash scripts/build_hven_pin.sh > "$WT/.scratch/train-$ARM-pin.log" 2>&1 || { echo PIN_BUILD_FAIL; tail -20 "$WT/.scratch/train-$ARM-pin.log"; exit 1; }
prefix="$HB/.hven/install-${PIN_SHA:0:12}"
bdir="$HB/.scratch/$br-build"
rm -rf "$bdir"
cmake -S "$HB" -B "$bdir" -G Ninja -Uhven_DIR -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_PREFIX_PATH="$prefix" > "$WT/.scratch/train-$ARM-configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -20 "$WT/.scratch/train-$ARM-configure.log"; exit 1; }
cmake --build "$bdir" --parallel 16 --target hven_bench_cell > "$WT/.scratch/train-$ARM-build.log" 2>&1 || { echo BUILD_FAIL; tail -20 "$WT/.scratch/train-$ARM-build.log"; exit 1; }
cp -p "$bdir/lanes/hven/hven_bench_cell" "$OUT/hven_bench_cell" || exit 1
grep -E 'HVEN_PIN_SHA' "$prefix/HVEN_PIN.txt"
sha256sum "$OUT/hven_bench_cell" "$prefix/lib64/libhven.a"
git log --oneline -1
echo "FOREGROUND_END $(date -Is)"
