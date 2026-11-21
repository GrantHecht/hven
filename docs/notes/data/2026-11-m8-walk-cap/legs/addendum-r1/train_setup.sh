#!/usr/bin/env bash
# M8 walk cap ADDENDUM-r1: ../train_setup.sh with out=build-wc/bin-r1 (I4 setup, not a leg): ../../2026-11-m8-dn3/r4/sl/flip_setup.sh re-pointed. The LOCAL
# hven-bench clone /home/ghecht/Projects/hven-bench-dn1 (never pushed; the main harness is not touched). ONE new branch
# from DN-3's flip lane commit (dn3-flip-legs, adcb7d2: DN-3's patches g, h, i + j, pinned to the flip 2d6d89cd0):
# pin -> the code sha, nothing else. scripts/build_hven_pin.sh (.hven/install-<sha12>), a configure into
# .scratch/<branch>-build (-Uhven_DIR), the lane binary frozen to build-wc/bin/hven_bench_cell. HEAD is put back on the
# branch found checked out; the new branch stays as the record.
# usage: train_setup.sh <code sha> <branch>   Run as: flock /tmp/box-build.lock bash train_setup.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a7a2ce81cf8988de5
HB=/home/ghecht/Projects/hven-bench-dn1
PIN_SHA=${1:?code sha}
br=${2:?branch}
out=$WT/build-wc/bin-r1
LANE_BASE=adcb7d2
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 TMPDIR=$WT/build-wc/tmp
mkdir -p "$TMPDIR"
[ "$(git -C "$WT" rev-parse "$PIN_SHA^{commit}")" = "$PIN_SHA" ] || { echo PIN_SHA_NOT_FULL; exit 4; }
cd "$HB" || exit 1
found=$(git rev-parse --abbrev-ref HEAD)
[ -z "$(git status --porcelain --untracked-files=no)" ] || { echo HB_DIRTY; git status --porcelain; exit 4; }
[ "$(git rev-parse --short=7 dn3-flip-legs)" = "$LANE_BASE" ] || { echo LANE_BASE; exit 4; }
git rev-parse --verify -q "$br" >/dev/null && { echo "BRANCH_EXISTS $br"; exit 4; }
git checkout --quiet -b "$br" dn3-flip-legs || exit 1
sed -i -e "s/^set(HVEN_PIN_SHA \".*\")$/set(HVEN_PIN_SHA \"$PIN_SHA\")/" \
       -e "s/^set(HVEN_PIN_BRANCH \".*\")$/set(HVEN_PIN_BRANCH \"m8-walk-cap\")/" cmake/hven_pin.cmake
git diff --stat
git -c user.name="Grant Hecht" -c user.email=grantrhecht@icloud.com commit --quiet -am \
  "EVIDENCE-ONLY (local clone, never pushed): pin hven ${PIN_SHA:0:9} (m8-walk-cap) (M8 walk cap TRAIN identity leg)" || exit 1
git log --oneline -1
bash scripts/build_hven_pin.sh > "$WT/build-wc/$br-pin.log" 2>&1 || { echo PIN_BUILD_FAIL; tail -20 "$WT/build-wc/$br-pin.log"; git checkout --quiet "$found"; exit 1; }
prefix="$HB/.hven/install-${PIN_SHA:0:12}"
bdir="$HB/.scratch/$br-build"
rm -rf "$bdir"
cmake -S "$HB" -B "$bdir" -G Ninja -Uhven_DIR -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_PREFIX_PATH="$prefix" > "$WT/build-wc/$br-configure.log" 2>&1 || { echo CONFIGURE_FAIL; git checkout --quiet "$found"; exit 1; }
cmake --build "$bdir" --parallel 16 --target hven_bench_cell > "$WT/build-wc/$br-build.log" 2>&1 || { echo BUILD_FAIL; tail -20 "$WT/build-wc/$br-build.log"; git checkout --quiet "$found"; exit 1; }
mkdir -p "$out"
cp -p "$bdir/lanes/hven/hven_bench_cell" "$out/hven_bench_cell" || exit 1
grep -E 'HVEN_PIN_SHA' "$prefix/HVEN_PIN.txt"
sha256sum "$out/hven_bench_cell" "$prefix/lib64/libhven.a"
git checkout --quiet "$found" || exit 1
git log --oneline -1
git status --porcelain --untracked-files=no
echo "FOREGROUND_END $(date -Is)"
