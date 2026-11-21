#!/usr/bin/env bash
# M8 DN-1 (../DECLARATION-dn1legs.txt): the local clone's commit B, after hb_setup_a.sh: hb_patch.py on
# lanes/hven/hven_bench_cell.cpp (dense-/t0- label prefixes, four side columns), committed; configured like
# configure.sh into .scratch/dn1-build-s (git-ignored; the pin's prefix, -Uhven_DIR) and built; the lane binary frozen
# to build-dn1r2/dn1bin/hven_bench_cell.S (the S and M legs run it). build/ (commit A's T binary) is not touched, and
# HEAD is returned to commit A (detached) so the T leg's runner stamps the commit its binary was built from.
# Run as: flock /tmp/box-build.lock bash hb_setup_b.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/dn1
D=$WT/docs/notes/data/2026-11-m8-dn1-impl/dn1legs
HB=/home/ghecht/Projects/hven-bench-dn1
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$HB" || exit 1
[ -z "$(git status --porcelain)" ] || { echo HB_DIRTY; git status --porcelain; exit 4; }
python3 "$D/hb_patch.py" lanes/hven/hven_bench_cell.cpp || exit 1
git diff --stat
git -c user.name="Grant Hecht" -c user.email=grantrhecht@icloud.com commit --quiet -am \
  "EVIDENCE-ONLY (local clone, never pushed): M8 DN-1 S/M legs -- dense-/t0- label prefixes and four side columns (hven docs/notes/data/2026-11-m8-dn1-impl/dn1legs/hb_patch.py)" || exit 1
git log --oneline -3
A=$(git rev-parse HEAD~1)
PREFIX="$HB/.hven/install-2171f8dd4233"
cmake -S "$HB" -B "$HB/.scratch/dn1-build-s" -G Ninja -Uhven_DIR -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_PREFIX_PATH="$PREFIX" > "$WT/build-dn1r2/hb-configure-b.log" 2>&1 || { echo CONFIGURE_FAIL; tail -20 "$WT/build-dn1r2/hb-configure-b.log"; exit 1; }
cmake --build "$HB/.scratch/dn1-build-s" --parallel 16 --target hven_bench_cell > "$WT/build-dn1r2/hb-build-b.log" 2>&1 || { echo BUILD_FAIL; tail -20 "$WT/build-dn1r2/hb-build-b.log"; exit 1; }
cp -p .scratch/dn1-build-s/lanes/hven/hven_bench_cell "$WT/build-dn1r2/dn1bin/hven_bench_cell.S" || exit 1
sha256sum "$WT/build-dn1r2/dn1bin/hven_bench_cell.S" "$WT/build-dn1r2/dn1bin/hven_bench_cell.T"
# Label smoke (no solve): every label the S and M legs use is accepted by both lanes' validate().
for lane in hven-ipm hven-sqp; do for lab in default dense-default t0-default t0-dense-default; do
  "$WT/build-dn1r2/dn1bin/hven_bench_cell.S" --lane $lane --problem hs071 --options $lab --dump-options-only \
    --options-dump "$WT/build-dn1r2/optdump-$lane-$lab.txt" | grep -E 'OPTIONS_(LABEL|VERDICT)'
done; done
cmp -s build/lanes/hven/hven_bench_cell "$WT/build-dn1r2/dn1bin/hven_bench_cell.T" && echo "build/ still the T binary"
git checkout --quiet --detach "$A" || exit 1
echo "HEAD $(git rev-parse HEAD) (commit A, for the T leg); branch dn1-legs at $(git rev-parse dn1-legs) (commit B)"
git status --porcelain
echo "FOREGROUND_END $(date -Is)"
