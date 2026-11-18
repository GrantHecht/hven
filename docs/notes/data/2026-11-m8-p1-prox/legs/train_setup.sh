#!/usr/bin/env bash
# TRAIN lane setup (not a leg): a PRIVATE clone of the local hven-bench (.scratch/hb, git-ignored, never pushed) at hven-bench main
# 6d1a8a07804964be524bbc9d5181f54228f7d400 + hb_patch_never.py (`never-<base>` = <base> with kNever) + the RS-1e include move
# (lanes/external/declared_point.h: detail/drivers -> detail/solver; docs/notes/data/2026-11-m8-rs/hven-bench-breaks.txt), pin -> <sha>.
# usage: train_setup.sh <label> <hven sha>   Run under: flock /tmp/box-build.lock setsid -w bash train_setup.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/p1
HB=$WT/.scratch/hb
LABEL=${1:?label}; PIN_SHA=${2:?hven sha (40 hex)}
SRC=/home/ghecht/Projects/hven-bench
HBMAIN=6d1a8a07804964be524bbc9d5181f54228f7d400
out=$WT/build-k/bin/$LABEL
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 TMPDIR=$WT/build-k/tmp
mkdir -p "$TMPDIR" "$out"
[ "$(git -C "$WT" rev-parse "$PIN_SHA^{commit}")" = "$PIN_SHA" ] || { echo PIN_SHA_NOT_FULL; exit 4; }
if [ ! -d "$HB/.git" ]; then
  git clone --quiet "$SRC" "$HB" || exit 1
  git -C "$HB" checkout --quiet -b p1-base "$HBMAIN" || exit 1
fi
mkdir -p "$HB/.scratch/h3" && { [ -d "$HB/.scratch/h3/decoded" ] || cp -r /home/ghecht/Projects/hven-bench-dn1/.scratch/h3/decoded "$HB/.scratch/h3/decoded"; } || exit 1
cd "$HB" || exit 1
export GIT_CEILING_DIRECTORIES=$WT/.scratch
br=p1-$LABEL
git rev-parse --verify -q "$br" > /dev/null && { echo "BRANCH_EXISTS $br"; exit 4; }
git checkout --quiet -b "$br" "$HBMAIN" || exit 1
python3 "$WT/.scratch/legs/hb_patch_never.py" lanes/hven/hven_bench_cell.cpp || exit 1
sed -i 's|#include <hven/detail/drivers/declared_diagnostics.h>|#include <hven/detail/solver/declared_diagnostics.h>|' lanes/external/declared_point.h
grep -q 'detail/solver/declared_diagnostics.h' lanes/external/declared_point.h || { echo INCLUDE_PATCH_FAIL; exit 1; }
sed -i -e "s/^set(HVEN_PIN_SHA \".*\")$/set(HVEN_PIN_SHA \"$PIN_SHA\")/" \
       -e "s/^set(HVEN_PIN_BRANCH \".*\")$/set(HVEN_PIN_BRANCH \"p1-$LABEL\")/" cmake/hven_pin.cmake
git diff --stat
git -c user.name="Grant Hecht" -c user.email=grantrhecht@icloud.com commit --quiet -am "EVIDENCE-ONLY (private clone, never pushed): pin hven ${PIN_SHA:0:9} (P-1 $LABEL)" || exit 1
bash scripts/build_hven_pin.sh > "$WT/build-k/train-$LABEL-pin.log" 2>&1 || { echo PIN_BUILD_FAIL; tail -20 "$WT/build-k/train-$LABEL-pin.log"; exit 1; }
prefix="$HB/.hven/install-${PIN_SHA:0:12}"
bdir="$HB/.scratch/$br-build"
rm -rf "$bdir"
cmake -S "$HB" -B "$bdir" -G Ninja -Uhven_DIR -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_PREFIX_PATH="$prefix" > "$WT/build-k/train-$LABEL-configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -20 "$WT/build-k/train-$LABEL-configure.log"; exit 1; }
cmake --build "$bdir" --parallel 16 --target hven_bench_cell > "$WT/build-k/train-$LABEL-build.log" 2>&1 || { echo BUILD_FAIL; tail -20 "$WT/build-k/train-$LABEL-build.log"; exit 1; }
cp -p "$bdir/lanes/hven/hven_bench_cell" "$out/hven_bench_cell" || exit 1
grep -E 'HVEN_PIN_SHA' "$prefix/HVEN_PIN.txt"
sha256sum "$out/hven_bench_cell"
git log --oneline -1
echo "FOREGROUND_END $(date -Is)"
