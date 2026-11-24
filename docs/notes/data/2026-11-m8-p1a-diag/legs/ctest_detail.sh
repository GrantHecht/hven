#!/usr/bin/env bash
WT=/home/ghecht/Projects/hven/.claude/worktrees/p1a-diag; O=$WT/.scratch/out/ctest; R=$(cat $O/union.re); B=HVEN_P1_
cd $WT/.scratch/rel
for arm in "P1live ${B}GLOBAL=1" "faceNSY ${B}MODE=face ${B}NOSAFE=1 ${B}NORUNG=1 ${B}YMAX=1e6" "faceNSYfp ${B}MODE=face ${B}NOSAFE=1 ${B}NORELRUNG=1 ${B}YMAX=1e6"; do
  set -- $arm; n=$1; shift
  env "$@" TMPDIR=$WT/.scratch/tmp ctest -j${BOX_LANE_JOBS:-4} --timeout 1800 --output-on-failure -R "$R" > $O/detail-$n.log 2>&1
  echo "$n $(grep 'tests passed' $O/detail-$n.log)"
done
