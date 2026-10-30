#!/usr/bin/env bash
# Install the scratch Release lib and build the replay tool against it. usage: build_tools.sh [extra cxx flags]
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9
S=$W/.scratch/e4
cmake --install $W/.scratch/nc2-build-release --prefix $S/install > $S/logs/install.log 2>&1 || exit 80
rm -rf $S/tools-build-replay
cmake -S $S/tools/replay -B $S/tools-build-replay -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
  "-DCMAKE_CXX_FLAGS=-DEIGEN_MAX_ALIGN_BYTES=32 $1" -DCMAKE_PREFIX_PATH=$S/install > $S/logs/cfg-replay.log 2>&1 || exit 81
cmake --build $S/tools-build-replay > $S/logs/build-replay.log 2>&1 || { tail -20 $S/logs/build-replay.log; exit 82; }
echo TOOLS_OK
