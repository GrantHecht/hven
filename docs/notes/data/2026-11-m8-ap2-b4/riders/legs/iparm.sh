#!/usr/bin/env bash
# AP-2 B4 riders: install the riders-head Release build, build the iparm dumper (a byte-for-byte copy of
# B4's, ../iparm/) against it, run it, and compare with B4's dump (../../iparm/iparm-b4.txt, the 15
# configurations at the B4 code head). Criterion: BYTE-IDENTICAL.
# Run as: flock /tmp/box-build.lock bash iparm.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-ac366926f941a970a
EV=$WT/docs/notes/data/2026-11-m8-ap2-b4/riders/iparm
PIN=$WT/docs/notes/data/2026-11-m8-ap2-b4/iparm/iparm-b4.txt
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
PREFIX=$WT/build-b4/inst-iparm
rm -rf "$PREFIX" "$WT/build-b4/iparm-build"
cmake --install "$WT/build-b4/rel" --prefix "$PREFIX" > "$WT/build-b4/install-iparm.log" 2>&1 || { echo INSTALL_FAIL; exit 1; }
cmake -S "$EV" -B "$WT/build-b4/iparm-build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_PREFIX_PATH="$PREFIX" -DHVEN_B3_ARM=1 \
  > "$WT/build-b4/iparm-build.configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -20 "$WT/build-b4/iparm-build.configure.log"; exit 1; }
cmake --build "$WT/build-b4/iparm-build" || { echo BUILD_FAIL; exit 1; }
"$WT/build-b4/iparm-build/dump_iparm" "$EV/iparm-riders.txt"
rc=$?
echo "dump rc=$rc"
if diff -q "$PIN" "$EV/iparm-riders.txt" > "$EV/diff.txt" 2>&1; then echo "IPARM_BYTE_IDENTICAL"; else echo "IPARM_DIFFERS"; diff "$PIN" "$EV/iparm-riders.txt" | head -20; rc=1; fi
sha256sum "$PIN" "$EV/iparm-riders.txt"
grep -c '^== config' "$EV/iparm-riders.txt"
echo "FOREGROUND_END $(date -Is)"
exit $rc
