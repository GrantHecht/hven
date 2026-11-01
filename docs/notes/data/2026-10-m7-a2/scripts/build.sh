#!/bin/bash
# usage: build.sh <rel|dbg> [targets...]
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-aa38d0451de957235
CFG=$1
shift
if [ "$CFG" = rel ]; then BT=Release; else BT=Debug; fi
B=$W/.scratch/a2/build-$CFG
L=$W/.scratch/a2/logs
mkdir -p "$L"
TG=""
for t in "$@"; do TG="$TG --target $t"; done
if pgrep -a hven_bench_cell; then echo PGREP_NONEMPTY; else echo PGREP_EMPTY; fi
echo FOREGROUND_START "$(date -Is)"
flock /tmp/box-build.lock bash -c "
set -o pipefail
cmake -S $W -B $B -G Ninja -DCMAKE_BUILD_TYPE=$BT -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DHVEN_BUILD_TESTS=ON -DHVEN_BUILD_BENCH=ON > $L/configure-$CFG.log 2>&1 &&
cmake --build $B -j14 $TG > $L/build-$CFG.log 2>&1
"
rc=$?
echo FOREGROUND_END rc=$rc "$(date -Is)"
grep -E 'error|FAILED' "$L/build-$CFG.log" | head -30
tail -3 "$L/build-$CFG.log"
exit $rc
