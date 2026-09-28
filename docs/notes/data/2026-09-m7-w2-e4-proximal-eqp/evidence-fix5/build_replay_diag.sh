#!/usr/bin/env bash
set -o pipefail
S=/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9/.scratch/e4-fix5
cmake --build $S/build-rel --parallel 16 > $S/logs/build-hven2.log 2>&1 || { tail -30 $S/logs/build-hven2.log; exit 82; }
cmake --install $S/build-rel > $S/logs/install-hven2.log 2>&1 || exit 83
P=$S/bench/.hven/install-ff02789b46a7
[ -d $S/replay-build-diag ] || cmake -S $S/replay -B $S/replay-build-diag -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_CXX_FLAGS="-DEIGEN_MAX_ALIGN_BYTES=32 -DF5_E4=1" -DCMAKE_PREFIX_PATH=$P \
  > $S/logs/cfg-replay-diag.log 2>&1 || { tail -20 $S/logs/cfg-replay-diag.log; exit 1; }
cmake --build $S/replay-build-diag > $S/logs/build-replay-diag.log 2>&1 || { tail -30 $S/logs/build-replay-diag.log; exit 2; }
echo REPLAY_DIAG_OK
R=$S/replay-build-diag/dump_replay
for arm in "1" "0"; do
  echo "----- E4 diag replay qp5, e2=$arm -----"
  env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 HVEN_F5_TRACE=1 taskset -c 2 $R --e2 $arm --algebra border $S/dumps/ACOPR118_e4/qp5.txt 2>&1 | sed 's|.*/qp5.txt|qp5|' | grep -E '^F5(TERM|WRONG|ESC|REF|SOLVE|ADDEXIT|POLISH)|^qp5' | head -25
done
