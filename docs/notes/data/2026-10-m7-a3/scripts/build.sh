#!/bin/bash
# usage: build.sh <base|head> <rel|dbg> [targets...]   -- one build under the box lock
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-afecbe0a5611f9eff
S=$W/.scratch/a3
WHICH=$1; CFG=$2; shift 2
if [ "$WHICH" = base ]; then SRC=$S/base-src; else SRC=$W; fi
if [ "$CFG" = rel ]; then BT=Release; else BT=Debug; fi
B=$S/build-$WHICH-$CFG
L=$S/logs
mkdir -p "$L"
TG=""
for t in "$@"; do TG="$TG --target $t"; done
echo "SRC $SRC HEAD $(git -C "$SRC" rev-parse HEAD) DIRTY=$(git -C "$SRC" status --porcelain --untracked-files=no | wc -l)"
if pgrep -a hven_bench_cell; then echo PGREP_NONEMPTY; else echo PGREP_EMPTY; fi
echo FOREGROUND_START "$(date -Is)"
flock /tmp/box-build.lock bash -c "
set -o pipefail
cmake -S $SRC -B $B -G Ninja -DCMAKE_BUILD_TYPE=$BT -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DHVEN_BUILD_TESTS=ON -DHVEN_BUILD_BENCH=ON > $L/configure-$WHICH-$CFG.log 2>&1 &&
cmake --build $B -j14 $TG > $L/build-$WHICH-$CFG.log 2>&1
"
rc=$?
echo FOREGROUND_END rc=$rc "$(date -Is)"
grep -E 'error:|FAILED' "$L/build-$WHICH-$CFG.log" | head -30
tail -2 "$L/build-$WHICH-$CFG.log"
exit $rc
