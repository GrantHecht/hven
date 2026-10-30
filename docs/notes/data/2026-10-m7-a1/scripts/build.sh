#!/bin/bash
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a026c6bee01193dcb
B=$W/.scratch/a1/build-rel
echo FOREGROUND_START $(date -Is)
flock /tmp/box-build.lock bash -c "
set -o pipefail
cmake -S $W -B $B -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DHVEN_BUILD_TESTS=ON > $W/.scratch/a1/configure.log 2>&1 &&
cmake --build $B -j8 --target hven_sqp_ipqp_e1_arm hven_sqp_tests > $W/.scratch/a1/build.log 2>&1
"
rc=$?
echo FOREGROUND_END rc=$rc $(date -Is)
