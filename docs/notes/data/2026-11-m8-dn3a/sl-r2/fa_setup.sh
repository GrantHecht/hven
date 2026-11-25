#!/usr/bin/env bash
# M8 DN-3a r2 (../DECLARATION-r2.txt, FA): ../sl/fa_setup.sh re-pointed for r2. The LOCAL hven-bench clone
# /home/ghecht/Projects/hven-bench-dn1 (never pushed). ONE new branch from DN-1d r3's commit E (dn1d-legs-r3):
#   dn3a-r2-legs    pin -> the r2 code head + hb_patch_g.py (the per-result refusal column; the renamed retake counter).
# The BASE lane binary is r1's, unchanged: build-dn3a/fabasebin/hven_bench_cell (branch dn3a-legs-base, pin b70e45cfb +
# hb_patch_f.py rename-only), checked by sha256 here and by fa_run.sh. scripts/build_hven_pin.sh
# (.hven/install-<sha12>), a configure into .scratch/dn3a-r2-legs-build (-Uhven_DIR), the lane binary frozen to
# build-dn3a/fabin-r2. HEAD is put back on dn1b-legs afterwards (as found); the branch stays as the record.
# usage: fa_setup.sh <code head sha>   Run as: flock /tmp/box-build.lock bash fa_setup.sh <sha>
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-acfc8f7d568381148
D=$WT/docs/notes/data/2026-11-m8-dn3a/sl-r2
HB=/home/ghecht/Projects/hven-bench-dn1
HEAD_SHA=${1:?code head sha}
COMMIT_E=f8c54bb
BASE_LANE_SHA=533c45cd2ee07728afa99b48837b6a46cca8a7e53e3cab493f327b9c78a27b48
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1
[ "$(git -C "$WT" rev-parse "$HEAD_SHA^{commit}")" = "$HEAD_SHA" ] || { echo CODE_HEAD; exit 4; }
[ "$(sha256sum "$WT/build-dn3a/fabasebin/hven_bench_cell" | cut -d' ' -f1)" = "$BASE_LANE_SHA" ] || { echo BASE_LANE_SHA; exit 4; }
cd "$HB" || exit 1
[ -z "$(git status --porcelain --untracked-files=no)" ] || { echo HB_DIRTY; git status --porcelain; exit 4; }
[ "$(git rev-parse --short=7 dn1d-legs-r3)" = "$COMMIT_E" ] || { echo COMMIT_E; exit 4; }
br=dn3a-r2-legs
out=$WT/build-dn3a/fabin-r2
git rev-parse --verify -q "$br" >/dev/null && { echo "BRANCH_EXISTS $br"; exit 4; }
git checkout --quiet -b "$br" dn1d-legs-r3 || exit 1
sed -i -e "s/^set(HVEN_PIN_SHA \".*\")$/set(HVEN_PIN_SHA \"$HEAD_SHA\")/" \
       -e "s/^set(HVEN_PIN_BRANCH \".*\")$/set(HVEN_PIN_BRANCH \"m8-dn3\")/" \
       -e 's/^set(HVEN_PIN_DATE ".*")$/set(HVEN_PIN_DATE "2026-11-12")/' cmake/hven_pin.cmake
python3 "$D/hb_patch_g.py" lanes/hven/hven_bench_cell.cpp || exit 1
git diff --stat
git -c user.name="Grant Hecht" -c user.email=grantrhecht@icloud.com commit --quiet -am \
  "EVIDENCE-ONLY (local clone, never pushed): pin hven ${HEAD_SHA:0:9} (m8-dn3) + hven docs/notes/data/2026-11-m8-dn3a/sl-r2/hb_patch_g.py (M8 DN-3a r2 FA leg)" || exit 1
git log --oneline -1
bash scripts/build_hven_pin.sh > "$WT/build-dn3a/$br-pin.log" 2>&1 || { echo PIN_BUILD_FAIL; tail -20 "$WT/build-dn3a/$br-pin.log"; exit 1; }
prefix="$HB/.hven/install-${HEAD_SHA:0:12}"
rm -rf "$HB/.scratch/$br-build"
cmake -S "$HB" -B "$HB/.scratch/$br-build" -G Ninja -Uhven_DIR -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_PREFIX_PATH="$prefix" > "$WT/build-dn3a/$br-configure.log" 2>&1 || { echo CONFIGURE_FAIL; exit 1; }
cmake --build "$HB/.scratch/$br-build" --parallel 16 --target hven_bench_cell > "$WT/build-dn3a/$br-build.log" 2>&1 || { echo BUILD_FAIL; tail -20 "$WT/build-dn3a/$br-build.log"; exit 1; }
mkdir -p "$out"
cp -p "$HB/.scratch/$br-build/lanes/hven/hven_bench_cell" "$out/hven_bench_cell" || exit 1
sha256sum "$out/hven_bench_cell"
grep -E 'HVEN_PIN_SHA' "$prefix/HVEN_PIN.txt"
git checkout --quiet dn1b-legs || exit 1
git log --oneline -1
git status --porcelain --untracked-files=no
echo "FOREGROUND_END $(date -Is)"
