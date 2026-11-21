#!/usr/bin/env bash
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf2b
B=$WT/.scratch/build-measC
export CCACHE_DISABLE=1 TMPDIR=$WT/.scratch/tmp
[ -f $B/build.ninja ] || cmake -S $WT/.scratch/measC -B $B -G Ninja -DCMAKE_BUILD_TYPE=Release -DHVEN_FP_MODE=SAFER_FAST \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_TESTS=OFF -DHVEN_BUILD_EXAMPLES=OFF > $B.cfg.log 2>&1 || { echo CFGFAIL; tail -20 $B.cfg.log; exit 1; }
ninja -C $B -j${BOX_LANE_JOBS:-4} hven_sqp_corpus > $B.build.log 2>&1; rc=$?; tail -5 $B.build.log; echo rc=$rc
