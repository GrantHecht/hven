#!/bin/bash
# usage: build.sh <rel|dbg> [targets...]   -- one build leg under the box lock
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a05b3c395d8efca12
CFG=$1
shift
if [ "$CFG" = rel ]; then BT=Release; else BT=Debug; fi
B=$W/.scratch/a7/build-$CFG
L=$W/.scratch/a7/logs
mkdir -p "$L"
TG=""
for t in "$@"; do TG="$TG --target $t"; done
echo FOREGROUND_START "$(date -Is)"
flock /tmp/box-build.lock bash -c "
set -o pipefail
if pgrep -a hven_bench_cell; then echo IN_LOCK_PGREP_NONEMPTY; else echo IN_LOCK_PGREP_EMPTY; fi
cmake -S $W -B $B -G Ninja -DCMAKE_BUILD_TYPE=$BT -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DHVEN_BUILD_TESTS=ON -DHVEN_BUILD_BENCH=ON > $L/configure-$CFG.log 2>&1 &&
cmake --build $B -j14 $TG > $L/build-$CFG.log 2>&1
"
rc=$?
echo FOREGROUND_END rc=$rc "$(date -Is)"
grep -E 'error|FAILED' "$L/build-$CFG.log" | head -30
tail -3 "$L/build-$CFG.log"
exit $rc
