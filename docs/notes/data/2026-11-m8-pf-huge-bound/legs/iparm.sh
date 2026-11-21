#!/usr/bin/env bash
# M8 PF-HB, copied from the B8 riders and re-pointed (worktree, build-pfe, this directory); was: AP-2 B8 riders (B7 riders' instrument; pin = B7 riders' iparm-b7r.txt): install the B8-head Release build, build the iparm dumper (a
# byte-for-byte copy of B5's, ../iparm/) against it, run it, and compare with B6's dump
# (B7's docs/notes/data/2026-11-m8-ap2-b7/iparm/iparm-b7.txt: the 15 configurations, itself
# byte-identical to B6's, B5's and B4's).
# Criterion: BYTE-IDENTICAL.
# Run as: flock /tmp/box-build.lock bash iparm.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-ab44eb20498de1642
SRC=$WT/docs/notes/data/2026-11-m8-pf-huge-bound/iparm
# The output directory may be given (B8_IPARM_EV); the dumper source stays SRC.
EV=${PFHB_IPARM_EV:-$SRC}
mkdir -p "$EV"
PIN=$WT/docs/notes/data/2026-11-m8-ap2-b8-riders/iparm/iparm-b8r.txt
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
PREFIX=$WT/build-pfe/inst-iparm
rm -rf "$PREFIX" "$WT/build-pfe/iparm-build"
cmake --install "$WT/build-pfe/rel" --prefix "$PREFIX" > "$WT/build-pfe/install-iparm.log" 2>&1 || { echo INSTALL_FAIL; exit 1; }
cmake -S "$SRC" -B "$WT/build-pfe/iparm-build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_PREFIX_PATH="$PREFIX" -DHVEN_B3_ARM=1 \
  > "$WT/build-pfe/iparm-build.configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -20 "$WT/build-pfe/iparm-build.configure.log"; exit 1; }
cmake --build "$WT/build-pfe/iparm-build" || { echo BUILD_FAIL; exit 1; }
"$WT/build-pfe/iparm-build/dump_iparm" "$EV/iparm-pfhb.txt"
rc=$?
echo "dump rc=$rc"
if diff -q "$PIN" "$EV/iparm-pfhb.txt" > "$EV/diff.txt" 2>&1; then echo "IPARM_BYTE_IDENTICAL"; else echo "IPARM_DIFFERS"; diff "$PIN" "$EV/iparm-pfhb.txt" | head -20; rc=1; fi
sha256sum "$PIN" "$EV/iparm-pfhb.txt"
grep -c '^== config' "$EV/iparm-pfhb.txt"
echo "FOREGROUND_END $(date -Is)"
exit $rc
