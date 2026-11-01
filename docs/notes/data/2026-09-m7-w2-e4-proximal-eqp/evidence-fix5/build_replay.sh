#!/usr/bin/env bash
set -o pipefail
S=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9/.scratch/e4-fix5
for tag in ff02789b46a7 85dc01ac9f53; do
  P=/home/ghecht/Projects/hven-bench/.hven/install-$tag
  if [ "$tag" = "ff02789b46a7" ]; then E4FLAG=1; else E4FLAG=0; fi
  [ -d "$P" ] || { echo "MISSING $P"; exit 9; }
  [ -d $S/replay-build-$tag ] || cmake -S $S/replay -B $S/replay-build-$tag -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_CXX_FLAGS="-DEIGEN_MAX_ALIGN_BYTES=32 -DF5_E4=${E4FLAG}" -DCMAKE_PREFIX_PATH=$P \
    > $S/logs/cfg-replay-$tag.log 2>&1 || { tail -20 $S/logs/cfg-replay-$tag.log; exit 1; }
  cmake --build $S/replay-build-$tag > $S/logs/build-replay-$tag.log 2>&1 \
    || { tail -30 $S/logs/build-replay-$tag.log; exit 2; }
  echo "REPLAY_BUILD_OK $tag"
done
