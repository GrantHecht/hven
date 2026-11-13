#!/usr/bin/env bash
# M8 DN-3 r4 (../../DECLARATION-r4.md): step 1 of ../../sl/adopt_setup.sh (no CFI build: r4 takes no perf record),
# re-pointed. The LOCAL hven-bench clone /home/ghecht/Projects/hven-bench-dn1 (never pushed; the main harness is not
# touched). ONE new branch from DN-3's lane commit (dn3-legs, 8722b90: patches g, h, i on DN-1d r3's commit E): pin ->
# the flip sha + hb_patch_j.py (the `never-` label prefix). scripts/build_hven_pin.sh (.hven/install-<sha12>), a
# configure into .scratch/<branch>-build (-Uhven_DIR), the lane binary frozen to build-flip/<out>/hven_bench_cell.
# HEAD is put back on the branch found checked out afterwards; the new branch stays as the record.
# usage: flip_setup.sh <flip sha> <branch> <out dir name>   Run as: flock /tmp/box-build.lock bash flip_setup.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a739270f237b3e30c
D=$WT/docs/notes/data/2026-11-m8-dn3/r4/sl
HB=/home/ghecht/Projects/hven-bench-dn1
PIN_SHA=${1:?flip sha}
br=${2:?branch}
out=$WT/build-flip/${3:?out dir name}
LANE_BASE=8722b90
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 TMPDIR=$WT/build-flip/tmp
[ "$(git -C "$WT" rev-parse "$PIN_SHA^{commit}")" = "$PIN_SHA" ] || { echo PIN_SHA_NOT_FULL; exit 4; }
cd "$HB" || exit 1
found=$(git rev-parse --abbrev-ref HEAD)
[ -z "$(git status --porcelain --untracked-files=no)" ] || { echo HB_DIRTY; git status --porcelain; exit 4; }
[ "$(git rev-parse --short=7 dn3-legs)" = "$LANE_BASE" ] || { echo LANE_BASE; exit 4; }
git rev-parse --verify -q "$br" >/dev/null && { echo "BRANCH_EXISTS $br"; exit 4; }
git checkout --quiet -b "$br" dn3-legs || exit 1
sed -i -e "s/^set(HVEN_PIN_SHA \".*\")$/set(HVEN_PIN_SHA \"$PIN_SHA\")/" \
       -e "s/^set(HVEN_PIN_BRANCH \".*\")$/set(HVEN_PIN_BRANCH \"m8-dn3-flip\")/" cmake/hven_pin.cmake
python3 "$D/hb_patch_j.py" lanes/hven/hven_bench_cell.cpp || { git checkout --quiet -- .; git checkout --quiet "$found"; exit 1; }
git diff --stat
git -c user.name="Grant Hecht" -c user.email=grantrhecht@icloud.com commit --quiet -am \
  "EVIDENCE-ONLY (local clone, never pushed): pin hven ${PIN_SHA:0:9} (m8-dn3-flip) + hven docs/notes/data/2026-11-m8-dn3/r4/sl/hb_patch_j.py (M8 DN-3 r4 flip identity leg)" || exit 1
git log --oneline -1
mkdir -p "$WT/build-flip"
bash scripts/build_hven_pin.sh > "$WT/build-flip/$br-pin.log" 2>&1 || { echo PIN_BUILD_FAIL; tail -20 "$WT/build-flip/$br-pin.log"; git checkout --quiet "$found"; exit 1; }
prefix="$HB/.hven/install-${PIN_SHA:0:12}"
bdir="$HB/.scratch/$br-build"
rm -rf "$bdir"
cmake -S "$HB" -B "$bdir" -G Ninja -Uhven_DIR -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_PREFIX_PATH="$prefix" > "$WT/build-flip/$br-configure.log" 2>&1 || { echo CONFIGURE_FAIL; git checkout --quiet "$found"; exit 1; }
cmake --build "$bdir" --parallel 16 --target hven_bench_cell > "$WT/build-flip/$br-build.log" 2>&1 || { echo BUILD_FAIL; tail -20 "$WT/build-flip/$br-build.log"; git checkout --quiet "$found"; exit 1; }
mkdir -p "$out"
cp -p "$bdir/lanes/hven/hven_bench_cell" "$out/hven_bench_cell" || exit 1
grep -E 'HVEN_PIN_SHA' "$prefix/HVEN_PIN.txt"
sha256sum "$out/hven_bench_cell" "$prefix/lib64/libhven.a"
git checkout --quiet "$found" || exit 1
git log --oneline -1
git status --porcelain --untracked-files=no
echo "FOREGROUND_END $(date -Is)"
