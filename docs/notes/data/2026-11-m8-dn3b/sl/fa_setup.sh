#!/usr/bin/env bash
# M8 DN-3b (../DECLARATION.txt, FA): structure from ../../2026-11-m8-dn3a/sl-r2/fa_setup.sh, re-pointed. The LOCAL
# hven-bench clone /home/ghecht/Projects/hven-bench-dn1 (never pushed). ONE new branch per call from DN-1d r3's commit E
# (dn1d-legs-r3, f8c54bb): pin -> <sha> + hb_patch_g.py (DN-3a r2's SQP column, byte copy) + hb_patch_h.py (the five IPM
# columns). scripts/build_hven_pin.sh (.hven/install-<sha12>), a configure into .scratch/<branch>-build (-Uhven_DIR), the
# lane binary frozen to build-dn3b/<out>/hven_bench_cell. HEAD is put back on dn1b-legs afterwards (as found); the branch
# stays as the record. Used for the BASE lane (ccd2e951f, branch dn3b-legs-base, out fabasebin) and the HEAD lane (the code
# head, branch dn3b-legs, out fabin); dev probes use other branch names (scratch, disclosed).
# usage: fa_setup.sh <hven sha> <branch> <out dir name>   Run as: flock /tmp/box-build.lock bash fa_setup.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a24a4442b0864282f
D=$WT/docs/notes/data/2026-11-m8-dn3b/sl
HB=/home/ghecht/Projects/hven-bench-dn1
PIN_SHA=${1:?hven sha}
br=${2:?branch}
out=$WT/build-dn3b/${3:?out dir name}
COMMIT_E=f8c54bb
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1
[ "$(git -C "$WT" rev-parse "$PIN_SHA^{commit}")" = "$PIN_SHA" ] || { echo PIN_SHA_NOT_FULL; exit 4; }
cd "$HB" || exit 1
[ -z "$(git status --porcelain --untracked-files=no)" ] || { echo HB_DIRTY; git status --porcelain; exit 4; }
[ "$(git rev-parse --short=7 dn1d-legs-r3)" = "$COMMIT_E" ] || { echo COMMIT_E; exit 4; }
git rev-parse --verify -q "$br" >/dev/null && { echo "BRANCH_EXISTS $br"; exit 4; }
git checkout --quiet -b "$br" dn1d-legs-r3 || exit 1
sed -i -e "s/^set(HVEN_PIN_SHA \".*\")$/set(HVEN_PIN_SHA \"$PIN_SHA\")/" \
       -e "s/^set(HVEN_PIN_BRANCH \".*\")$/set(HVEN_PIN_BRANCH \"m8-dn3b\")/" \
       -e 's/^set(HVEN_PIN_DATE ".*")$/set(HVEN_PIN_DATE "2026-11-12")/' cmake/hven_pin.cmake
python3 "$D/hb_patch_g.py" lanes/hven/hven_bench_cell.cpp || { git checkout --quiet -- .; git checkout --quiet dn1b-legs; exit 1; }
python3 "$D/hb_patch_h.py" lanes/hven/hven_bench_cell.cpp || { git checkout --quiet -- .; git checkout --quiet dn1b-legs; exit 1; }
git diff --stat
git -c user.name="Grant Hecht" -c user.email=grantrhecht@icloud.com commit --quiet -am \
  "EVIDENCE-ONLY (local clone, never pushed): pin hven ${PIN_SHA:0:9} (m8-dn3b) + hven docs/notes/data/2026-11-m8-dn3b/sl/hb_patch_g.py + hb_patch_h.py (M8 DN-3b FA leg)" || exit 1
git log --oneline -1
mkdir -p "$WT/build-dn3b"
bash scripts/build_hven_pin.sh > "$WT/build-dn3b/$br-pin.log" 2>&1 || { echo PIN_BUILD_FAIL; tail -20 "$WT/build-dn3b/$br-pin.log"; git checkout --quiet dn1b-legs; exit 1; }
prefix="$HB/.hven/install-${PIN_SHA:0:12}"
rm -rf "$HB/.scratch/$br-build"
cmake -S "$HB" -B "$HB/.scratch/$br-build" -G Ninja -Uhven_DIR -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_PREFIX_PATH="$prefix" > "$WT/build-dn3b/$br-configure.log" 2>&1 || { echo CONFIGURE_FAIL; git checkout --quiet dn1b-legs; exit 1; }
cmake --build "$HB/.scratch/$br-build" --parallel 16 --target hven_bench_cell > "$WT/build-dn3b/$br-build.log" 2>&1 || { echo BUILD_FAIL; tail -20 "$WT/build-dn3b/$br-build.log"; git checkout --quiet dn1b-legs; exit 1; }
mkdir -p "$out"
cp -p "$HB/.scratch/$br-build/lanes/hven/hven_bench_cell" "$out/hven_bench_cell" || exit 1
sha256sum "$out/hven_bench_cell"
grep -E 'HVEN_PIN_SHA' "$prefix/HVEN_PIN.txt"
git checkout --quiet dn1b-legs || exit 1
git log --oneline -1
git status --porcelain --untracked-files=no
echo "FOREGROUND_END $(date -Is)"
