#!/usr/bin/env bash
# M8 F1 (copied from PF-1, re-pointed: worktree f1, build-f1, this directory, arm A 2340b95d). Was: M8 PF-1, copied from PF-HB (which copied the B8 riders) and re-pointed (worktree, build-f1, this directory); was: AP-2 B8 riders (B7 riders' instrument; pin = B7 riders' iparm-b7r.txt): install the B8-head Release build, build the iparm dumper (a
# byte-for-byte copy of B5's, ../iparm/) against it, run it, and compare with B6's dump
# (B7's docs/notes/data/2026-11-m8-ap2-b7/iparm/iparm-b7.txt: the 15 configurations, itself
# byte-identical to B6's, B5's and B4's).
# Criterion: BYTE-IDENTICAL.
# Run as: flock /tmp/box-build.lock bash iparm.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/f1
SRC=$WT/docs/notes/data/2026-11-m8-f1-init/iparm
# The output directory may be given (B8_IPARM_EV); the dumper source stays SRC.
EV=${F1_IPARM_EV:-$SRC}
mkdir -p "$EV"
PIN=$WT/docs/notes/data/2026-11-m8-ap2-b8-riders/iparm/iparm-b8r.txt
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
PREFIX=$WT/build-f1/inst-iparm
rm -rf "$PREFIX" "$WT/build-f1/iparm-build"
cmake --install "$WT/build-f1/rel" --prefix "$PREFIX" > "$WT/build-f1/install-iparm.log" 2>&1 || { echo INSTALL_FAIL; exit 1; }
cmake -S "$SRC" -B "$WT/build-f1/iparm-build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_PREFIX_PATH="$PREFIX" -DHVEN_B3_ARM=1 \
  > "$WT/build-f1/iparm-build.configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -20 "$WT/build-f1/iparm-build.configure.log"; exit 1; }
cmake --build "$WT/build-f1/iparm-build" || { echo BUILD_FAIL; exit 1; }
"$WT/build-f1/iparm-build/dump_iparm" "$EV/iparm-f1.txt"
rc=$?
echo "dump rc=$rc"
if diff -q "$PIN" "$EV/iparm-f1.txt" > "$EV/diff.txt" 2>&1; then echo "IPARM_BYTE_IDENTICAL"; else echo "IPARM_DIFFERS"; diff "$PIN" "$EV/iparm-f1.txt" | head -20; rc=1; fi
sha256sum "$PIN" "$EV/iparm-f1.txt"
grep -c '^== config' "$EV/iparm-f1.txt"
echo "FOREGROUND_END $(date -Is)"
exit $rc
