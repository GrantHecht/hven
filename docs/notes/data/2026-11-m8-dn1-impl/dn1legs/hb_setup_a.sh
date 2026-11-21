#!/usr/bin/env bash
# M8 DN-1 (../DECLARATION-dn1legs.txt): the LOCAL hven-bench clone, commit A (the T leg's harness). Never pushed;
# /home/ghecht/Projects/hven-bench and hven-bench-sd are not written. Clone of hven-bench-sd at bcf1d92 (= hven-bench
# main), branch dn1-legs; the CUTEst SIF files copied in (git-ignored, as in hven-bench-sd); commit A = the pin moved
# to the DN-1 code head (cmake/hven_pin.cmake only; PF-1's pattern); scripts/build_hven_pin.sh; configure.sh; build;
# the lane binary frozen to build-dn1r2/dn1bin/hven_bench_cell.T.
# Run as: flock /tmp/box-build.lock bash hb_setup_a.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/dn1
HB=/home/ghecht/Projects/hven-bench-dn1
SRC=/home/ghecht/Projects/hven-bench-sd
HEAD_SHA=2171f8dd4233b1bc21682fc250060294e7d42a3d
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1
[ -e "$HB" ] && { echo "HB_EXISTS $HB"; exit 4; }
[ "$(git -C "$SRC" rev-parse HEAD)" = bcf1d92da7f5469902786e71fb6e1700ebd446af ] || { echo SRC_HEAD; exit 4; }
git clone --quiet "$SRC" "$HB" || exit 1
cd "$HB" || exit 1
git remote remove origin
git checkout --quiet -b dn1-legs bcf1d92da7f5469902786e71fb6e1700ebd446af || exit 1
mkdir -p sets/cutest && cp -a "$SRC/sets/cutest/sif" sets/cutest/sif || exit 1
sed -i -e "s/^set(HVEN_PIN_SHA \".*\")$/set(HVEN_PIN_SHA \"$HEAD_SHA\")/" \
       -e 's/^set(HVEN_PIN_BRANCH ".*")$/set(HVEN_PIN_BRANCH "m8-dn1-impl")/' \
       -e 's/^set(HVEN_PIN_DATE ".*")$/set(HVEN_PIN_DATE "2026-11-09")/' cmake/hven_pin.cmake
git diff --stat
git -c user.name="Grant Hecht" -c user.email=grantrhecht@icloud.com commit --quiet -am \
  "EVIDENCE-ONLY (local clone, never pushed): pin hven 2171f8dd4 (M8 DN-1 code head, branch m8-dn1-impl) for DN-1's T leg" || exit 1
git log --oneline -2
bash scripts/build_hven_pin.sh || exit 1
bash configure.sh > "$WT/build-dn1r2/hb-configure-a.log" 2>&1 || { echo CONFIGURE_FAIL; tail -20 "$WT/build-dn1r2/hb-configure-a.log"; exit 1; }
cmake --build build --parallel 16 > "$WT/build-dn1r2/hb-build-a.log" 2>&1 || { echo BUILD_FAIL; tail -20 "$WT/build-dn1r2/hb-build-a.log"; exit 1; }
tail -2 "$WT/build-dn1r2/hb-build-a.log"
mkdir -p "$WT/build-dn1r2/dn1bin"
cp -p build/lanes/hven/hven_bench_cell "$WT/build-dn1r2/dn1bin/hven_bench_cell.T" || exit 1
sha256sum build/lanes/hven/hven_bench_cell "$WT/build-dn1r2/dn1bin/hven_bench_cell.T" ".hven/install-${HEAD_SHA:0:12}/lib/libhven.a"
cat ".hven/install-${HEAD_SHA:0:12}/HVEN_PIN.txt"
git status --porcelain
echo "FOREGROUND_END $(date -Is)"
