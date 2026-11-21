#!/usr/bin/env bash
# M8 DN-1c S and T legs (../DECLARATION.txt): the LOCAL hven-bench clone /home/ghecht/Projects/hven-bench-dn1 (never
# pushed; hven-bench and hven-bench-sd are not written). Structure from ../../2026-11-m8-dn1b-guard/sl/hb_setup_c.sh.
# Branch dn1c-legs from DN-1b's commit C 1b03c78 (branch dn1b-legs: DN-1's commit B + the IPM side column
# dn1b_dense_residual_ratio_triggers); commit D = the pin moved to the DN-1c code head (cmake/hven_pin.cmake only) +
# hb_patch_d.py (one SQP side column); then scripts/build_hven_pin.sh (.hven/install-<head12>), a configure into
# .scratch/dn1c-build (the pin's prefix, -Uhven_DIR), the lane binary built and frozen to build-dn1ce/dn1cbin/. build/ is
# not touched. HEAD is left at commit D during the S and T legs; legs/hb_restore.sh puts the clone back on dn1b-legs.
# usage: hb_setup_d.sh <code head sha>   Run as: flock /tmp/box-build.lock bash hb_setup_d.sh <sha>
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/dn1c
D=$WT/docs/notes/data/2026-11-m8-dn1c-sqp-check/sl
HB=/home/ghecht/Projects/hven-bench-dn1
HEAD_SHA=${1:?code head sha}
COMMIT_C=1b03c78
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1
[ "$(git -C "$WT" rev-parse "$HEAD_SHA^{commit}")" = "$HEAD_SHA" ] || { echo CODE_HEAD; exit 4; }
cd "$HB" || exit 1
[ -z "$(git status --porcelain --untracked-files=no)" ] || { echo HB_DIRTY; git status --porcelain; exit 4; }
[ "$(git rev-parse --short=7 dn1b-legs)" = "$COMMIT_C" ] || { echo COMMIT_C; exit 4; }
git rev-parse --verify -q dn1c-legs >/dev/null && { echo BRANCH_EXISTS; exit 4; }
git checkout --quiet -b dn1c-legs dn1b-legs || exit 1
sed -i -e "s/^set(HVEN_PIN_SHA \".*\")$/set(HVEN_PIN_SHA \"$HEAD_SHA\")/" \
       -e 's/^set(HVEN_PIN_BRANCH ".*")$/set(HVEN_PIN_BRANCH "m8-dn1c")/' \
       -e 's/^set(HVEN_PIN_DATE ".*")$/set(HVEN_PIN_DATE "2026-11-10")/' cmake/hven_pin.cmake
python3 "$D/hb_patch_d.py" lanes/hven/hven_bench_cell.cpp || exit 1
git diff --stat
git -c user.name="Grant Hecht" -c user.email=grantrhecht@icloud.com commit --quiet -am \
  "EVIDENCE-ONLY (local clone, never pushed): pin hven ${HEAD_SHA:0:9} (M8 DN-1c code head, branch m8-dn1c) + one SQP side column (hven docs/notes/data/2026-11-m8-dn1c-sqp-check/sl/hb_patch_d.py)" || exit 1
git log --oneline -3
bash scripts/build_hven_pin.sh || exit 1
PREFIX="$HB/.hven/install-${HEAD_SHA:0:12}"
mkdir -p "$WT/build-dn1ce/dn1cbin"
cmake -S "$HB" -B "$HB/.scratch/dn1c-build" -G Ninja -Uhven_DIR -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_PREFIX_PATH="$PREFIX" > "$WT/build-dn1ce/hb-configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -20 "$WT/build-dn1ce/hb-configure.log"; exit 1; }
cmake --build "$HB/.scratch/dn1c-build" --parallel 16 --target hven_bench_cell > "$WT/build-dn1ce/hb-build.log" 2>&1 || { echo BUILD_FAIL; tail -20 "$WT/build-dn1ce/hb-build.log"; exit 1; }
cp -p "$HB/.scratch/dn1c-build/lanes/hven/hven_bench_cell" "$WT/build-dn1ce/dn1cbin/hven_bench_cell" || exit 1
sha256sum "$WT/build-dn1ce/dn1cbin/hven_bench_cell" "$PREFIX/lib/libhven.a" 2>/dev/null || sha256sum "$WT/build-dn1ce/dn1cbin/hven_bench_cell"
cat "$PREFIX/HVEN_PIN.txt"
for lane in hven-ipm hven-sqp; do for lab in default dense-default; do
  "$WT/build-dn1ce/dn1cbin/hven_bench_cell" --lane $lane --problem hs071 --options $lab --dump-options-only \
    --options-dump "$WT/build-dn1ce/optdump-$lane-$lab.txt" | grep -E 'OPTIONS_(LABEL|VERDICT)'
done; done
git status --porcelain --untracked-files=no
echo "FOREGROUND_END $(date -Is)"
