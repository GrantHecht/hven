#!/usr/bin/env bash
# TRAIN lane setup (not a leg): a PRIVATE clone of the local hven-bench clone (.scratch/hb, git-ignored, never pushed), the lane
# branch dn3-flip-legs (adcb7d2: DN-3's patches g, h, i, j; the `default` / `never-default` labels), pin -> <sha>, one lane binary per arm.
# usage: train_setup.sh <A|B> <hven sha>   Run under: flock /tmp/box-build.lock setsid -w bash train_setup.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/kopt-1x
HB=$WT/.scratch/hb
ARM=${1:?A|B}; PIN_SHA=${2:?hven sha (40 hex)}
SRC=/home/ghecht/Projects/hven-bench-dn1
out=$WT/build-k/bin/$ARM
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 TMPDIR=$WT/build-k/tmp GIT_CEILING_DIRECTORIES=$WT/build-k/tmp
mkdir -p "$TMPDIR" "$out"
[ "$(git -C "$WT" rev-parse "$PIN_SHA^{commit}")" = "$PIN_SHA" ] || { echo PIN_SHA_NOT_FULL; exit 4; }
if [ ! -d "$HB/.git" ]; then
  git clone --quiet "$SRC" "$HB" || exit 1
  git -C "$HB" checkout --quiet -b "kopt1x-base" adcb7d2 || exit 1
fi
cd "$HB" || exit 1
br=kopt1x-$ARM
git rev-parse --verify -q "$br" > /dev/null && { echo "BRANCH_EXISTS $br"; exit 4; }
git checkout --quiet -b "$br" adcb7d2 || exit 1
sed -i -e "s/^set(HVEN_PIN_SHA \".*\")$/set(HVEN_PIN_SHA \"$PIN_SHA\")/" \
       -e "s/^set(HVEN_PIN_BRANCH \".*\")$/set(HVEN_PIN_BRANCH \"kopt-1x-$ARM\")/" cmake/hven_pin.cmake
git diff --stat
git -c user.name="Grant Hecht" -c user.email=grantrhecht@icloud.com commit --quiet -am "EVIDENCE-ONLY (private clone, never pushed): pin hven ${PIN_SHA:0:9} (kopt-1x arm $ARM)" || exit 1
bash scripts/build_hven_pin.sh > "$WT/build-k/train-$ARM-pin.log" 2>&1 || { echo PIN_BUILD_FAIL; tail -20 "$WT/build-k/train-$ARM-pin.log"; exit 1; }
prefix="$HB/.hven/install-${PIN_SHA:0:12}"
bdir="$HB/.scratch/$br-build"
rm -rf "$bdir"
cmake -S "$HB" -B "$bdir" -G Ninja -Uhven_DIR -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_PREFIX_PATH="$prefix" > "$WT/build-k/train-$ARM-configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -20 "$WT/build-k/train-$ARM-configure.log"; exit 1; }
cmake --build "$bdir" --parallel 16 --target hven_bench_cell > "$WT/build-k/train-$ARM-build.log" 2>&1 || { echo BUILD_FAIL; tail -20 "$WT/build-k/train-$ARM-build.log"; exit 1; }
cp -p "$bdir/lanes/hven/hven_bench_cell" "$out/hven_bench_cell" || exit 1
grep -E 'HVEN_PIN_SHA' "$prefix/HVEN_PIN.txt"
sha256sum "$out/hven_bench_cell" "$prefix/lib64/libhven.a"
git log --oneline -1
echo "FOREGROUND_END $(date -Is)"
