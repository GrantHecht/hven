#!/usr/bin/env bash
# WA-1: ../../2026-11-m8-kopt-1x-r2/legs/train_setup.sh re-pointed (this worktree, build-wa/), branch names wa1-*, plus the RS include fix and the sets/cutest copy (gitignored in the source clone).
# TRAIN lane setup (not a leg): a PRIVATE clone of the local hven-bench clone (.scratch/hb, git-ignored, never pushed), the lane
# current hven-bench main 6d1a8a0 + hb_patch_never.py (the `never-<base>` prefix; `default` / `never-default`), pin -> <sha>, one lane binary per arm.
# usage: train_setup.sh <A|B> <hven sha>   Run under: flock /tmp/box-build.lock setsid -w bash train_setup.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/wa1
HB=$WT/.scratch/hb
ARM=${1:?A|B}; PIN_SHA=${2:?hven sha (40 hex)}
SRC=/home/ghecht/Projects/hven-bench   # r2: the CURRENT lane (main 6d1a8a07804964be524bbc9d5181f54228f7d400); the dn3-flip-legs lane no longer compiles against this hven's API
out=$WT/build-wa/bin/$ARM
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 TMPDIR=$WT/build-wa/tmp GIT_CEILING_DIRECTORIES=$WT/build-wa/tmp
mkdir -p "$TMPDIR" "$out"
[ "$(git -C "$WT" rev-parse "$PIN_SHA^{commit}")" = "$PIN_SHA" ] || { echo PIN_SHA_NOT_FULL; exit 4; }
if [ ! -d "$HB/.git" ]; then
  git clone --quiet "$SRC" "$HB" || exit 1
  git -C "$HB" checkout --quiet -b "wa1-base" 6d1a8a07804964be524bbc9d5181f54228f7d400 || exit 1
fi
[ -d "$HB/sets/cutest" ] || cp -r "$SRC/sets/cutest" "$HB/sets/cutest" || exit 1
mkdir -p "$HB/.scratch/h3" && [ -d "$HB/.scratch/h3/decoded" ] || cp -r /home/ghecht/Projects/hven-bench-dn1/.scratch/h3/decoded "$HB/.scratch/h3/decoded" || exit 1
cd "$HB" || exit 1
br=wa1-$ARM
git rev-parse --verify -q "$br" > /dev/null && { echo "BRANCH_EXISTS $br"; exit 4; }
git checkout --quiet -b "$br" 6d1a8a07804964be524bbc9d5181f54228f7d400 || exit 1
python3 "$WT/docs/notes/data/2026-11-m8-wa1/legs/hb_patch_never.py" lanes/hven/hven_bench_cell.cpp || exit 1
# the RS-era include path (hven RS-3 moved detail/drivers/ to detail/solver/); the lane's one detail include
sed -i 's#<hven/detail/drivers/declared_diagnostics.h>#<hven/detail/solver/declared_diagnostics.h>#' lanes/external/declared_point.h || exit 1
grep -q '<hven/detail/solver/declared_diagnostics.h>' lanes/external/declared_point.h || { echo RS_INCLUDE_FIX_FAIL; exit 1; }
sed -i -e "s/^set(HVEN_PIN_SHA \".*\")$/set(HVEN_PIN_SHA \"$PIN_SHA\")/" \
       -e "s/^set(HVEN_PIN_BRANCH \".*\")$/set(HVEN_PIN_BRANCH \"wa-1-$ARM\")/" cmake/hven_pin.cmake
git diff --stat
git -c user.name="Grant Hecht" -c user.email=grantrhecht@icloud.com commit --quiet -am "EVIDENCE-ONLY (private clone, never pushed): pin hven ${PIN_SHA:0:9} (wa-1 arm $ARM)" || exit 1
bash scripts/build_hven_pin.sh > "$WT/build-wa/train-$ARM-pin.log" 2>&1 || { echo PIN_BUILD_FAIL; tail -20 "$WT/build-wa/train-$ARM-pin.log"; exit 1; }
prefix="$HB/.hven/install-${PIN_SHA:0:12}"
bdir="$HB/.scratch/$br-build"
rm -rf "$bdir"
cmake -S "$HB" -B "$bdir" -G Ninja -Uhven_DIR -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_PREFIX_PATH="$prefix" > "$WT/build-wa/train-$ARM-configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -20 "$WT/build-wa/train-$ARM-configure.log"; exit 1; }
cmake --build "$bdir" --parallel 16 --target hven_bench_cell > "$WT/build-wa/train-$ARM-build.log" 2>&1 || { echo BUILD_FAIL; tail -20 "$WT/build-wa/train-$ARM-build.log"; exit 1; }
cp -p "$bdir/lanes/hven/hven_bench_cell" "$out/hven_bench_cell" || exit 1
grep -E 'HVEN_PIN_SHA' "$prefix/HVEN_PIN.txt"
sha256sum "$out/hven_bench_cell" "$prefix/lib64/libhven.a"
git log --oneline -1
echo "FOREGROUND_END $(date -Is)"
