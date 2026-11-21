#!/usr/bin/env bash
# M8 dense gate (../DECLARATION-r2.md): DN-3 r4's sl/flip_setup.sh re-pointed. The LOCAL hven-bench clone
# /home/ghecht/Projects/hven-bench-dn1 (never pushed; the main harness is not touched). ONE new branch from DN-3's flip
# lane commit dn3-flip-legs (adcb7d2: DN-3's lane + hb_patch_j's `never-` prefix); the ONLY change is the pin -> the
# dense-gate code head. scripts/build_hven_pin.sh (.hven/install-<sha12>, cloned from ~/Projects/hven, which holds this
# worktree's commits), a configure into .scratch/<branch>-build (-Uhven_DIR), the lane binary frozen to
# build-dg/<out>/hven_bench_cell. HEAD is put back on the branch found checked out; the new branch stays as the record.
# usage: lane_setup.sh <code head sha> <branch> <out dir name>   Run as: flock /tmp/box-build.lock bash lane_setup.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a44d4824b0beb0ea7
HB=/home/ghecht/Projects/hven-bench-dn1
PIN_SHA=${1:?code head sha}
br=${2:?branch}
out=$WT/build-dg/${3:?out dir name}
LANE_BASE=adcb7d2
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 TMPDIR=$WT/build-dg/tmp
[ "$(git -C "$WT" rev-parse "$PIN_SHA^{commit}")" = "$PIN_SHA" ] || { echo PIN_SHA_NOT_FULL; exit 4; }
cd "$HB" || exit 1
found=$(git rev-parse --abbrev-ref HEAD)
[ -z "$(git status --porcelain --untracked-files=no)" ] || { echo HB_DIRTY; git status --porcelain; exit 4; }
[ "$(git rev-parse --short=7 dn3-flip-legs)" = "$LANE_BASE" ] || { echo LANE_BASE; exit 4; }
git rev-parse --verify -q "$br" >/dev/null && { echo "BRANCH_EXISTS $br"; exit 4; }
git checkout --quiet -b "$br" dn3-flip-legs || exit 1
sed -i -e "s/^set(HVEN_PIN_SHA \".*\")$/set(HVEN_PIN_SHA \"$PIN_SHA\")/" \
       -e "s/^set(HVEN_PIN_BRANCH \".*\")$/set(HVEN_PIN_BRANCH \"m8-dense-gate\")/" cmake/hven_pin.cmake
git diff --stat
git -c user.name="Grant Hecht" -c user.email=grantrhecht@icloud.com commit --quiet -am \
  "EVIDENCE-ONLY (local clone, never pushed): pin hven ${PIN_SHA:0:9} (m8-dense-gate) on dn3-flip-legs (M8 dense gate TRAIN lane)" || exit 1
git log --oneline -1
mkdir -p "$WT/build-dg"
bash scripts/build_hven_pin.sh > "$WT/build-dg/$br-pin.log" 2>&1 || { echo PIN_BUILD_FAIL; tail -20 "$WT/build-dg/$br-pin.log"; git checkout --quiet "$found"; exit 1; }
prefix="$HB/.hven/install-${PIN_SHA:0:12}"
bdir="$HB/.scratch/$br-build"
rm -rf "$bdir"
cmake -S "$HB" -B "$bdir" -G Ninja -Uhven_DIR -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_PREFIX_PATH="$prefix" > "$WT/build-dg/$br-configure.log" 2>&1 || { echo CONFIGURE_FAIL; git checkout --quiet "$found"; exit 1; }
cmake --build "$bdir" --parallel 16 --target hven_bench_cell > "$WT/build-dg/$br-build.log" 2>&1 || { echo BUILD_FAIL; tail -20 "$WT/build-dg/$br-build.log"; git checkout --quiet "$found"; exit 1; }
mkdir -p "$out"
cp -p "$bdir/lanes/hven/hven_bench_cell" "$out/hven_bench_cell" || exit 1
grep -E 'HVEN_PIN_SHA' "$prefix/HVEN_PIN.txt"
sha256sum "$out/hven_bench_cell" "$prefix/lib64/libhven.a"
git checkout --quiet "$found" || exit 1
git log --oneline -1
git status --porcelain --untracked-files=no
echo "FOREGROUND_END $(date -Is)"
