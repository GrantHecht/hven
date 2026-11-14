#!/usr/bin/env bash
# M8 DN-3 (../DECLARATION-r1.md): structure from ../../2026-11-m8-dn3b/sl/fa_setup.sh (the lane branch and pin build) and
# ../../2026-11-m8-dn0-ipm-r1/tools/build_instruments.sh steps 1, 2 and 5 (the CFI record build and its identity
# proof), re-pointed. The LOCAL hven-bench clone /home/ghecht/Projects/hven-bench-dn1 (never pushed).
#  1. ONE new branch from DN-1d r3's commit E (dn1d-legs-r3, f8c54bb): pin -> <sha> + hb_patch_g.py (DN-3a r2's SQP column)
#     + hb_patch_h.py (DN-3b's five IPM columns) + hb_patch_i.py (the `auto-` label prefix, kAuto; the two
#     dn3_dense_factorizations columns). scripts/build_hven_pin.sh (.hven/install-<sha12>), a configure into
#     .scratch/<branch>-build (-Uhven_DIR), the lane binary frozen to build-dn3/<out>/hven_bench_cell (the STAT binary).
#  2. The CFI hven: .hven/src (the pin, as build_hven_pin.sh left it) configured with build_hven_pin.sh's cmake arguments
#     plus -DCCACHE_PROGRAM=cfi_launcher.sh (appends -fasynchronous-unwind-tables, CFI metadata only), installed into
#     .scratch/<branch>-cfi/install.
#  3. The RECORD binary: the stat binary's own link command (ninja -t commands) with ONLY libhven.a's path and the output
#     swapped, frozen to build-dn3/<out>/hven_bench_cell.cfi.
#  4. code_identity.py: every executable section of libhven.a (pin vs CFI) byte-identical, else exit 5 (STOP S-A1).
# HEAD is put back on dn1b-legs afterwards (as found); the branch stays as the record.
# usage: adopt_setup.sh <hven sha> <branch> <out dir name>   Run as: flock /tmp/box-build.lock bash adopt_setup.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a4a349c1df0402cb8
D=$WT/docs/notes/data/2026-11-m8-dn3/sl
HB=/home/ghecht/Projects/hven-bench-dn1
PIN_SHA=${1:?hven sha}
br=${2:?branch}
out=$WT/build-dn3/${3:?out dir name}
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
       -e "s/^set(HVEN_PIN_BRANCH \".*\")$/set(HVEN_PIN_BRANCH \"m8-dn3\")/" \
       -e 's/^set(HVEN_PIN_DATE ".*")$/set(HVEN_PIN_DATE "2026-11-13")/' cmake/hven_pin.cmake
for pt in g h i; do
  python3 "$D/hb_patch_$pt.py" lanes/hven/hven_bench_cell.cpp || { git checkout --quiet -- .; git checkout --quiet dn1b-legs; exit 1; }
done
git diff --stat
git -c user.name="Grant Hecht" -c user.email=grantrhecht@icloud.com commit --quiet -am \
  "EVIDENCE-ONLY (local clone, never pushed): pin hven ${PIN_SHA:0:9} (m8-dn3) + hven docs/notes/data/2026-11-m8-dn3/sl/hb_patch_g.py + hb_patch_h.py + hb_patch_i.py (M8 DN-3 enumeration and adoption legs)" || exit 1
git log --oneline -1
mkdir -p "$WT/build-dn3"
bash scripts/build_hven_pin.sh > "$WT/build-dn3/$br-pin.log" 2>&1 || { echo PIN_BUILD_FAIL; tail -20 "$WT/build-dn3/$br-pin.log"; git checkout --quiet dn1b-legs; exit 1; }
prefix="$HB/.hven/install-${PIN_SHA:0:12}"
bdir="$HB/.scratch/$br-build"
rm -rf "$bdir"
cmake -S "$HB" -B "$bdir" -G Ninja -Uhven_DIR -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_PREFIX_PATH="$prefix" > "$WT/build-dn3/$br-configure.log" 2>&1 || { echo CONFIGURE_FAIL; git checkout --quiet dn1b-legs; exit 1; }
cmake --build "$bdir" --parallel 16 --target hven_bench_cell > "$WT/build-dn3/$br-build.log" 2>&1 || { echo BUILD_FAIL; tail -20 "$WT/build-dn3/$br-build.log"; git checkout --quiet dn1b-legs; exit 1; }
mkdir -p "$out"
cp -p "$bdir/lanes/hven/hven_bench_cell" "$out/hven_bench_cell" || exit 1
grep -E 'HVEN_PIN_SHA' "$prefix/HVEN_PIN.txt"

# 2. the CFI hven, from the pin's own source tree
[ "$(git -C "$HB/.hven/src" rev-parse HEAD)" = "$PIN_SHA" ] || { echo CFI_SRC_NOT_PIN; git checkout --quiet dn1b-legs; exit 1; }
[ -z "$(git -C "$HB/.hven/src" status --porcelain --untracked-files=all)" ] || { echo CFI_SRC_DIRTY; git checkout --quiet dn1b-legs; exit 1; }
cfi="$HB/.scratch/$br-cfi"
rm -rf "$cfi"
cmake -S "$HB/.hven/src" -B "$cfi/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=clang++ -DHVEN_BUILD_TESTS=OFF -DHVEN_BUILD_BENCH=OFF -DHVEN_LINK_TIME_OPT=OFF \
      -DCMAKE_INSTALL_PREFIX="$cfi/install" \
      -DCCACHE_PROGRAM="$D/cfi_launcher.sh" > "$WT/build-dn3/$br-cfi.configure.log" 2>&1 || { echo CFI_CONFIGURE_FAIL; git checkout --quiet dn1b-legs; exit 1; }
cmake --build "$cfi/build" --parallel 16 > "$WT/build-dn3/$br-cfi.build.log" 2>&1 || { echo CFI_BUILD_FAIL; git checkout --quiet dn1b-legs; exit 1; }
cmake --install "$cfi/build" > "$WT/build-dn3/$br-cfi.install.log" 2>&1 || { echo CFI_INSTALL_FAIL; git checkout --quiet dn1b-legs; exit 1; }
nw="$(cd "$cfi/build" && ninja -t commands | grep -c "cfi_launcher.sh" || true)"
echo "launcher-wrapped compile commands: $nw"
[ "$nw" -gt 0 ] || { echo CFI_LAUNCHER_WRAPS_NOTHING; git checkout --quiet dn1b-legs; exit 1; }

# 3. the RECORD binary: the stat binary's link command with libhven.a swapped
cmd="$(cd "$bdir" && ninja -t commands lanes/hven/hven_bench_cell | tail -1)"
[[ "$cmd" == *"$prefix/lib64/libhven.a"* ]] || { echo LINK_CMD_NO_PIN_LIB; git checkout --quiet dn1b-legs; exit 1; }
new="${cmd//${prefix}\/lib64\/libhven.a/${cfi}/install/lib64/libhven.a}"
new="${new// -o lanes\/hven\/hven_bench_cell / -o ${out}/hven_bench_cell.cfi }"
new="${new//--dependency-file=lanes\/hven\/CMakeFiles\/hven_bench_cell.dir\/link.d/--dependency-file=${out}/cfi-link.d}"
printf '%s\n' "$cmd" > "$out/link.stat.txt"
printf '%s\n' "$new" > "$out/link.cfi.txt"
(cd "$bdir" && bash -c "$new") || { echo CFI_LINK_FAIL; git checkout --quiet dn1b-legs; exit 1; }
diff <(tr ' ' '\n' < "$out/link.stat.txt") <(tr ' ' '\n' < "$out/link.cfi.txt") > "$out/link.diff"
cat "$out/link.diff"

# 4. identity: libhven.a's executable sections, pin vs CFI; the two installs' header trees
python3 "$D/code_identity.py" "$prefix/lib64/libhven.a" "$cfi/install/lib64/libhven.a" "$out/identity-ar" \
  > "$out/libhven-code-identity.txt"
idrc=$?
tail -1 "$out/libhven-code-identity.txt"
diff -r "$prefix/include" "$cfi/install/include" > "$out/include-diff.txt" && echo INCLUDE_TREES_IDENTICAL
sha256sum "$out/hven_bench_cell" "$out/hven_bench_cell.cfi" "$prefix/lib64/libhven.a" "$cfi/install/lib64/libhven.a"
git checkout --quiet dn1b-legs || exit 1
git log --oneline -1
git status --porcelain --untracked-files=no
echo "FOREGROUND_END $(date -Is)"
[ "$idrc" -eq 0 ] || { echo "CODE_IDENTITY_FAILED (STOP S-A1)"; exit 5; }
