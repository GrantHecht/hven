#!/usr/bin/env bash
# AP-2 B7 (B5's instrument): install the B7-head Release build, build the iparm dumper (a
# byte-for-byte copy of B5's, ../iparm/) against it, run it, and compare with B6's dump
# (B6's docs/notes/data/2026-11-m8-ap2-b6/iparm/iparm-b6.txt, which the B6 riders reproduced
# byte-identically: the 15 configurations, itself byte-identical to B5's and B4's).
# Criterion: BYTE-IDENTICAL.
# Run as: flock /tmp/box-build.lock bash iparm.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a00603e2048c1c37b
SRC=$WT/docs/notes/data/2026-11-m8-ap2-b7/iparm
# The output directory may be given (B7_IPARM_EV); the dumper source stays SRC.
EV=${B7_IPARM_EV:-$SRC}
mkdir -p "$EV"
PIN=$WT/docs/notes/data/2026-11-m8-ap2-b6/iparm/iparm-b6.txt
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
PREFIX=$WT/build-b7e/inst-iparm
rm -rf "$PREFIX" "$WT/build-b7e/iparm-build"
cmake --install "$WT/build-b7e/rel" --prefix "$PREFIX" > "$WT/build-b7e/install-iparm.log" 2>&1 || { echo INSTALL_FAIL; exit 1; }
cmake -S "$SRC" -B "$WT/build-b7e/iparm-build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_PREFIX_PATH="$PREFIX" -DHVEN_B3_ARM=1 \
  > "$WT/build-b7e/iparm-build.configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -20 "$WT/build-b7e/iparm-build.configure.log"; exit 1; }
cmake --build "$WT/build-b7e/iparm-build" || { echo BUILD_FAIL; exit 1; }
"$WT/build-b7e/iparm-build/dump_iparm" "$EV/iparm-b7.txt"
rc=$?
echo "dump rc=$rc"
if diff -q "$PIN" "$EV/iparm-b7.txt" > "$EV/diff.txt" 2>&1; then echo "IPARM_BYTE_IDENTICAL"; else echo "IPARM_DIFFERS"; diff "$PIN" "$EV/iparm-b7.txt" | head -20; rc=1; fi
sha256sum "$PIN" "$EV/iparm-b7.txt"
grep -c '^== config' "$EV/iparm-b7.txt"
echo "FOREGROUND_END $(date -Is)"
exit $rc
