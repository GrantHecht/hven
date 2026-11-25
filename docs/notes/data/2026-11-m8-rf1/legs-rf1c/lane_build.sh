#!/usr/bin/env bash
# RF-1: copied from ../../2026-11-m8-pf2d/legs/lane_build.sh, re-pointed to this worktree (paths, branch names); no other change.
# PF-4 lane binary build (not a leg): WA-1's train_setup.sh re-pointed. A PRIVATE clone of the local hven-bench clone
# (.scratch/hb, git-ignored, never pushed) at hven-bench main e03cbad (pin 48e1eeae; the PF-3 harness), pin -> <sha>,
# one lane binary per tag into .scratch/bin/<tag>/hven_bench_cell. NEVER=1 applies hb_patch_never.py (WA-1's; the
# `never-<base>` label = <base> with linear.dense_factorization = kNever) -- the TRAIN lanes; the step-1 probe lane is unpatched.
# usage: lane_build.sh <tag> <hven sha (40 hex)>   Run under: setsid -w flock /tmp/box-build.lock bash lane_build.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/rf1c
HB=/home/ghecht/Projects/hven/.scratch/rf1c-build-legs/hb
TAG=${1:?tag}; PIN_SHA=${2:?hven sha (40 hex)}
SRC=/home/ghecht/Projects/hven-bench
BASE=e03cbad
out=/home/ghecht/Projects/hven/.scratch/rf1c-build-legs/bin/$TAG
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 TMPDIR=/home/ghecht/Projects/hven/.scratch/rf1c-build-legs/tmp GIT_CEILING_DIRECTORIES=/home/ghecht/Projects/hven/.scratch/rf1c-build-legs/tmp
mkdir -p "$TMPDIR" "$out"
[ "$(git -C "$WT" rev-parse "$PIN_SHA^{commit}")" = "$PIN_SHA" ] || { echo PIN_SHA_NOT_FULL; exit 4; }
if [ ! -d "$HB/.git" ]; then
  git clone --quiet "$SRC" "$HB" || exit 1
fi
[ -d "$HB/sets/cutest" ] || cp -r "$SRC/sets/cutest" "$HB/sets/cutest" || exit 1
mkdir -p "$HB/.scratch/h3" && { [ -d "$HB/.scratch/h3/decoded" ] || cp -r /home/ghecht/Projects/hven-bench-pf3/.scratch/h3/decoded "$HB/.scratch/h3/decoded"; } || exit 1
cd "$HB" || exit 1
br=rf1-$TAG-${PIN_SHA:0:9}
git rev-parse --verify -q "$br" > /dev/null && { echo "BRANCH_EXISTS $br"; exit 4; }
git checkout --quiet -b "$br" "$BASE" || exit 1
if [ "${NEVER:-0}" = 1 ]; then python3 "$WT/docs/notes/data/2026-11-m8-rf1/legs-rf1c/hb_patch_never.py" lanes/hven/hven_bench_cell.cpp || exit 1; fi
sed -i -e "s/^set(HVEN_PIN_SHA \".*\")$/set(HVEN_PIN_SHA \"$PIN_SHA\")/" \
       -e "s/^set(HVEN_PIN_BRANCH \".*\")$/set(HVEN_PIN_BRANCH \"rf1-$TAG\")/" cmake/hven_pin.cmake
git diff --stat
git -c user.name="Grant Hecht" -c user.email=grantrhecht@icloud.com commit --quiet -am "EVIDENCE-ONLY (private clone, never pushed): pin hven ${PIN_SHA:0:9} (rf1 $TAG)" || exit 1
bash scripts/build_hven_pin.sh > "$out/pin.log" 2>&1 || { echo PIN_BUILD_FAIL; tail -20 "$out/pin.log"; exit 1; }
prefix="$HB/.hven/install-${PIN_SHA:0:12}"
bdir="$HB/.scratch/$br-build"
rm -rf "$bdir"
cmake -S "$HB" -B "$bdir" -G Ninja -Uhven_DIR -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_PREFIX_PATH="$prefix" > "$out/configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -20 "$out/configure.log"; exit 1; }
cmake --build "$bdir" --parallel ${BOX_LANE_JOBS:-4} --target hven_bench_cell > "$out/build.log" 2>&1 || { echo BUILD_FAIL; tail -20 "$out/build.log"; exit 1; }
cp -p "$bdir/lanes/hven/hven_bench_cell" "$out/hven_bench_cell" || exit 1
grep -E 'HVEN_PIN_SHA' "$prefix/HVEN_PIN.txt"
sha256sum "$out/hven_bench_cell" "$prefix"/lib*/libhven.a
git log --oneline -1
echo "FOREGROUND_END $(date -Is)"
