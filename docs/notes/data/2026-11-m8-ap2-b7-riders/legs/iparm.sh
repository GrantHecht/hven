#!/usr/bin/env bash
# AP-2 B7 riders (B5's instrument): install the B7-head Release build, build the iparm dumper (a
# byte-for-byte copy of B5's, ../iparm/) against it, run it, and compare with B6's dump
# (B7's docs/notes/data/2026-11-m8-ap2-b7/iparm/iparm-b7.txt: the 15 configurations, itself
# byte-identical to B6's, B5's and B4's).
# Criterion: BYTE-IDENTICAL.
# Run as: flock /tmp/box-build.lock bash iparm.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a00603e2048c1c37b
SRC=$WT/docs/notes/data/2026-11-m8-ap2-b7-riders/iparm
# The output directory may be given (B7R_IPARM_EV); the dumper source stays SRC.
EV=${B7R_IPARM_EV:-$SRC}
mkdir -p "$EV"
PIN=$WT/docs/notes/data/2026-11-m8-ap2-b7/iparm/iparm-b7.txt
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
PREFIX=$WT/build-b7r/inst-iparm
rm -rf "$PREFIX" "$WT/build-b7r/iparm-build"
cmake --install "$WT/build-b7r/rel" --prefix "$PREFIX" > "$WT/build-b7r/install-iparm.log" 2>&1 || { echo INSTALL_FAIL; exit 1; }
cmake -S "$SRC" -B "$WT/build-b7r/iparm-build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_PREFIX_PATH="$PREFIX" -DHVEN_B3_ARM=1 \
  > "$WT/build-b7r/iparm-build.configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -20 "$WT/build-b7r/iparm-build.configure.log"; exit 1; }
cmake --build "$WT/build-b7r/iparm-build" || { echo BUILD_FAIL; exit 1; }
"$WT/build-b7r/iparm-build/dump_iparm" "$EV/iparm-b7r.txt"
rc=$?
echo "dump rc=$rc"
if diff -q "$PIN" "$EV/iparm-b7r.txt" > "$EV/diff.txt" 2>&1; then echo "IPARM_BYTE_IDENTICAL"; else echo "IPARM_DIFFERS"; diff "$PIN" "$EV/iparm-b7r.txt" | head -20; rc=1; fi
sha256sum "$PIN" "$EV/iparm-b7r.txt"
grep -c '^== config' "$EV/iparm-b7r.txt"
echo "FOREGROUND_END $(date -Is)"
exit $rc
