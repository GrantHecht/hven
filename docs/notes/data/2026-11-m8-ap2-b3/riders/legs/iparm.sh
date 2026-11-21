#!/usr/bin/env bash
# AP-2 B3 riders, B3-a: install the riders-head Release build, build the (riders copy of the)
# iparm dumper against it, run it, and compare with the B3 pin dump with the two configurations
# whose option is gone (algorithm=two_level, backend_print=on) removed.
# Run as: flock /tmp/box-build.lock bash iparm.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a3b067b098f5e9221
EV=$WT/docs/notes/data/2026-11-m8-ap2-b3/riders/iparm
PIN=$WT/docs/notes/data/2026-11-m8-ap2-b3/iparm/iparm-pin.txt
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
PREFIX=$WT/build-r/inst-iparm
rm -rf "$PREFIX" "$WT/build-r/iparm-build"
cmake --install "$WT/build-b3/rel" --prefix "$PREFIX" > "$WT/build-r/install-iparm.log" 2>&1 || { echo INSTALL_FAIL; exit 1; }
cmake -S "$EV" -B "$WT/build-r/iparm-build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_PREFIX_PATH="$PREFIX" -DHVEN_B3_ARM=1 \
  > "$WT/build-r/iparm-build.configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -20 "$WT/build-r/iparm-build.configure.log"; exit 1; }
cmake --build "$WT/build-r/iparm-build" || { echo BUILD_FAIL; exit 1; }
"$WT/build-r/iparm-build/dump_iparm" "$EV/iparm-riders.txt"
rc=$?
echo "dump rc=$rc"
python3 - "$PIN" "$EV/iparm-pin-filtered.txt" <<'PY'
import sys
drop = {'algorithm=two_level', 'backend_print=on'}
out, keep = [], True
for line in open(sys.argv[1]):
    if line.startswith('== config '):
        keep = line.split()[2] not in drop
    if keep:
        out.append(line)
open(sys.argv[2], 'w').writelines(out)
PY
if diff -q "$EV/iparm-pin-filtered.txt" "$EV/iparm-riders.txt" > "$EV/diff.txt" 2>&1; then echo "IPARM_BYTE_IDENTICAL"; else echo "IPARM_DIFFERS"; diff "$EV/iparm-pin-filtered.txt" "$EV/iparm-riders.txt" | head -20; rc=1; fi
sha256sum "$EV/iparm-pin-filtered.txt" "$EV/iparm-riders.txt"
grep -c '^== config' "$EV/iparm-riders.txt"
echo "FOREGROUND_END $(date -Is)"
exit $rc
