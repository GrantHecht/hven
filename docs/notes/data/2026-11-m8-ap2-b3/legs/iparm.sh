#!/usr/bin/env bash
# AP-2 B3-a: install one arm's build, build the iparm dumper against it, run it.
# usage: iparm.sh <arm: pin|b3> <build-dir-name>
# Run as: flock /tmp/box-build.lock bash iparm.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a3b067b098f5e9221
EV=$WT/docs/notes/data/2026-11-m8-ap2-b3/iparm
ARM=${1:?pin|b3}
DIR=$WT/build-b3/${2:?build dir}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
case $ARM in pin) DEF=0 ;; b3) DEF=1 ;; *) exit 2 ;; esac
PREFIX=$WT/build-b3/inst-$ARM
rm -rf "$PREFIX" "$WT/build-b3/iparm-$ARM"
cmake --install "$DIR" --prefix "$PREFIX" > "$WT/build-b3/install-$ARM.log" 2>&1 || { echo INSTALL_FAIL; exit 1; }
cmake -S "$EV" -B "$WT/build-b3/iparm-$ARM" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_PREFIX_PATH="$PREFIX" -DHVEN_B3_ARM=$DEF \
  > "$WT/build-b3/iparm-$ARM.configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -20 "$WT/build-b3/iparm-$ARM.configure.log"; exit 1; }
cmake --build "$WT/build-b3/iparm-$ARM" || { echo BUILD_FAIL; exit 1; }
"$WT/build-b3/iparm-$ARM/dump_iparm" "$EV/iparm-$ARM.txt"
rc=$?
echo "dump rc=$rc"
grep -c '^call' "$EV/iparm-$ARM.txt"
grep -E '^(==|status|REFUSED)' "$EV/iparm-$ARM.txt"
echo "FOREGROUND_END $(date -Is)"
exit $rc
