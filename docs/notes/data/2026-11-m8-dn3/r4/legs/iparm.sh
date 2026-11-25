#!/usr/bin/env bash
# M8 DN-3 r4 (../../DECLARATION-r4.md, I3a gating / I3b reported): DN-1d r3's legs/iparm.sh re-pointed. The flip tree's
# Release build is installed once; two dumpers are built against it with -DHVEN_B3_ARM=1:
#   I3a ../iparm-never/ (B8 riders' dumper, kNever set explicitly) -> iparm-never.txt, byte-compared with the B8-riders
#       pin docs/notes/data/2026-11-m8-ap2-b8-riders/iparm/iparm-b8r.txt;
#   I3b the B8 riders' dumper unchanged (the flipped default)      -> iparm-default.txt, read against r4's stated
#       expectation (reported only).
# usage: iparm.sh <evidence-dir>   Run as: flock /tmp/box-build.lock bash iparm.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a739270f237b3e30c
EV=${1:?evidence dir}
NEVER_SRC=$WT/docs/notes/data/2026-11-m8-dn3/r4/iparm-never
B8R_SRC=$WT/docs/notes/data/2026-11-m8-ap2-b8-riders/iparm
PIN=$B8R_SRC/iparm-b8r.txt
B=$WT/build-flip
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 TMPDIR=$B/tmp
mkdir -p "$EV"
PREFIX=$B/inst-iparm
rm -rf "$PREFIX" "$B/iparm-never-build" "$B/iparm-default-build"
cmake --install "$B/rel" --prefix "$PREFIX" > "$B/install-iparm.log" 2>&1 || { echo INSTALL_FAIL; exit 1; }
rc=0
for arm in never default; do
  src=$NEVER_SRC; [ "$arm" = default ] && src=$B8R_SRC
  cmake -S "$src" -B "$B/iparm-$arm-build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_PREFIX_PATH="$PREFIX" -DHVEN_B3_ARM=1 \
    > "$B/iparm-$arm.configure.log" 2>&1 || { echo "CONFIGURE_FAIL $arm"; tail -20 "$B/iparm-$arm.configure.log"; exit 1; }
  cmake --build "$B/iparm-$arm-build" > "$B/iparm-$arm.build.log" 2>&1 || { echo "BUILD_FAIL $arm"; tail -20 "$B/iparm-$arm.build.log"; exit 1; }
  "$B/iparm-$arm-build/dump_iparm" "$EV/iparm-$arm.txt"; drc=$?
  echo "dump $arm rc=$drc"; [ $drc -ne 0 ] && rc=1
done
if cmp -s "$PIN" "$EV/iparm-never.txt"; then echo "I3a IPARM_BYTE_IDENTICAL"; else echo "I3a IPARM_DIFFERS"; diff "$PIN" "$EV/iparm-never.txt" | head -20; rc=1; fi
sha256sum "$PIN" "$EV/iparm-never.txt" "$EV/iparm-default.txt"
echo "I3b (reported): the flipped default's dump, status lines per config:"
grep -E '^== config|^status=|^REFUSED' "$EV/iparm-default.txt" | paste - - | sed 's/^/  /'
echo "I3b pardiso calls recorded in total: $(grep -c '^call ' "$EV/iparm-default.txt")"
echo "FOREGROUND_END $(date -Is)"
exit $rc
