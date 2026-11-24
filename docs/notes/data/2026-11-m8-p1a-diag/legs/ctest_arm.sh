#!/usr/bin/env bash
# usage: box-lane -- setsid -w bash ctest_arm.sh NAME [K=V ...]
WT=/home/ghecht/Projects/hven/.claude/worktrees/p1a-diag; N=$1; shift
O=$WT/.scratch/out/ctest; mkdir -p $O
cd $WT/.scratch/rel && env "$@" TMPDIR=$WT/.scratch/tmp ctest --output-on-failure -j${BOX_LANE_JOBS:-4} --timeout 1800 > $O/$N.log 2>&1
echo "ctest $N rc=$? $(grep 'tests passed' $O/$N.log)"
