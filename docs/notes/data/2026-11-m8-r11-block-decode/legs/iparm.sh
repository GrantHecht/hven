#!/usr/bin/env bash
# M8 R11: copied from F1 (2026-11-m8-f1-init; runtime from its runtime-r2, period 400000; census legs from PF-1) and re-pointed: worktree r11, build-r11e, this directory, arm A a8c540cf.
# M8 F1 (copied from PF-1, re-pointed: worktree f1, build-f1, this directory, arm A 2340b95d). Was: M8 PF-1, copied from PF-HB (which copied the B8 riders) and re-pointed (worktree, build-f1, this directory); was: AP-2 B8 riders (B7 riders' instrument; pin = B7 riders' iparm-b7r.txt): install the B8-head Release build, build the iparm dumper (a
# byte-for-byte copy of B5's, ../iparm/) against it, run it, and compare with B6's dump
# (B7's docs/notes/data/2026-11-m8-ap2-b7/iparm/iparm-b7.txt: the 15 configurations, itself
# byte-identical to B6's, B5's and B4's).
# Criterion: BYTE-IDENTICAL.
# Run as: flock /tmp/box-build.lock bash iparm.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/r11
SRC=$WT/docs/notes/data/2026-11-m8-r11-block-decode/iparm
# The output directory may be given (B8_IPARM_EV); the dumper source stays SRC.
EV=${R11_IPARM_EV:-$SRC}
mkdir -p "$EV"
PIN=$WT/docs/notes/data/2026-11-m8-ap2-b8-riders/iparm/iparm-b8r.txt
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
PREFIX=$WT/build-r11e/inst-iparm
rm -rf "$PREFIX" "$WT/build-r11e/iparm-build"
cmake --install "$WT/build-r11e/rel" --prefix "$PREFIX" > "$WT/build-r11e/install-iparm.log" 2>&1 || { echo INSTALL_FAIL; exit 1; }
cmake -S "$SRC" -B "$WT/build-r11e/iparm-build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_PREFIX_PATH="$PREFIX" -DHVEN_B3_ARM=1 \
  > "$WT/build-r11e/iparm-build.configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -20 "$WT/build-r11e/iparm-build.configure.log"; exit 1; }
cmake --build "$WT/build-r11e/iparm-build" || { echo BUILD_FAIL; exit 1; }
"$WT/build-r11e/iparm-build/dump_iparm" "$EV/iparm-r11.txt"
rc=$?
echo "dump rc=$rc"
if diff -q "$PIN" "$EV/iparm-r11.txt" > "$EV/diff.txt" 2>&1; then echo "IPARM_BYTE_IDENTICAL"; else echo "IPARM_DIFFERS"; diff "$PIN" "$EV/iparm-r11.txt" | head -20; rc=1; fi
sha256sum "$PIN" "$EV/iparm-r11.txt"
grep -c '^== config' "$EV/iparm-r11.txt"
echo "FOREGROUND_END $(date -Is)"
exit $rc
