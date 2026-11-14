#!/usr/bin/env bash
# M8 DN-1b S and T legs (../DECLARATION.txt): the LOCAL hven-bench clone /home/ghecht/Projects/hven-bench-dn1 (never
# pushed; hven-bench and hven-bench-sd are not written). Branch dn1b-legs from DN-1's commit B 5751a59 (dense-/t0-
# label prefixes and four side columns, ../../2026-11-m8-dn1-impl/dn1legs/hb_patch.py); commit C = the pin moved to
# the DN-1b code head (cmake/hven_pin.cmake only, PF-1's pattern) + hb_patch_c.py (one side column); then
# scripts/build_hven_pin.sh (.hven/install-<head12>), a configure like configure.sh into .scratch/dn1b-build (the
# pin's prefix, -Uhven_DIR) and the lane binary built and frozen to build-dn1be/dn1bbin/hven_bench_cell. build/ is
# not touched. HEAD is left at commit C (the T runner stamps the commit its binary was built from).
# usage: hb_setup_c.sh <code head sha>   Run as: flock /tmp/box-build.lock bash hb_setup_c.sh <sha>
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/dn1b
D=$WT/docs/notes/data/2026-11-m8-dn1b-guard/sl
HB=/home/ghecht/Projects/hven-bench-dn1
HEAD_SHA=${1:?code head sha}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1
[ "$(git -C "$WT" rev-parse "$HEAD_SHA^{commit}")" = "$HEAD_SHA" ] || { echo CODE_HEAD; exit 4; }
cd "$HB" || exit 1
[ -z "$(git status --porcelain --untracked-files=no)" ] || { echo HB_DIRTY; git status --porcelain; exit 4; }
[ "$(git rev-parse dn1-legs)" = 5751a59f0fe0873b869bd477a8bbf02ce752d5b6 ] || { echo COMMIT_B; exit 4; }
git rev-parse --verify -q dn1b-legs >/dev/null && { echo BRANCH_EXISTS; exit 4; }
git checkout --quiet -b dn1b-legs 5751a59f0fe0873b869bd477a8bbf02ce752d5b6 || exit 1
sed -i -e "s/^set(HVEN_PIN_SHA \".*\")$/set(HVEN_PIN_SHA \"$HEAD_SHA\")/" \
       -e 's/^set(HVEN_PIN_BRANCH ".*")$/set(HVEN_PIN_BRANCH "m8-dn1b")/' \
       -e 's/^set(HVEN_PIN_DATE ".*")$/set(HVEN_PIN_DATE "2026-11-09")/' cmake/hven_pin.cmake
python3 "$D/hb_patch_c.py" lanes/hven/hven_bench_cell.cpp || exit 1
git diff --stat
git -c user.name="Grant Hecht" -c user.email=grantrhecht@icloud.com commit --quiet -am \
  "EVIDENCE-ONLY (local clone, never pushed): pin hven ${HEAD_SHA:0:9} (M8 DN-1b code head, branch m8-dn1b) + one side column (hven docs/notes/data/2026-11-m8-dn1b-guard/sl/hb_patch_c.py)" || exit 1
git log --oneline -3
bash scripts/build_hven_pin.sh || exit 1
PREFIX="$HB/.hven/install-${HEAD_SHA:0:12}"
mkdir -p "$WT/build-dn1be/dn1bbin"
cmake -S "$HB" -B "$HB/.scratch/dn1b-build" -G Ninja -Uhven_DIR -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_PREFIX_PATH="$PREFIX" > "$WT/build-dn1be/hb-configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -20 "$WT/build-dn1be/hb-configure.log"; exit 1; }
cmake --build "$HB/.scratch/dn1b-build" --parallel 16 --target hven_bench_cell > "$WT/build-dn1be/hb-build.log" 2>&1 || { echo BUILD_FAIL; tail -20 "$WT/build-dn1be/hb-build.log"; exit 1; }
cp -p "$HB/.scratch/dn1b-build/lanes/hven/hven_bench_cell" "$WT/build-dn1be/dn1bbin/hven_bench_cell" || exit 1
sha256sum "$WT/build-dn1be/dn1bbin/hven_bench_cell" "$PREFIX/lib/libhven.a" 2>/dev/null || sha256sum "$WT/build-dn1be/dn1bbin/hven_bench_cell"
cat "$PREFIX/HVEN_PIN.txt"
for lane in hven-ipm hven-sqp; do for lab in default dense-default; do
  "$WT/build-dn1be/dn1bbin/hven_bench_cell" --lane $lane --problem hs071 --options $lab --dump-options-only \
    --options-dump "$WT/build-dn1be/optdump-$lane-$lab.txt" | grep -E 'OPTIONS_(LABEL|VERDICT)'
done; done
git status --porcelain --untracked-files=no
echo "FOREGROUND_END $(date -Is)"
