#!/usr/bin/env bash
WT=/home/ghecht/Projects/hven/.claude/worktrees/p1a-diag; L=$WT/.scratch/legs; B=HVEN_P1_
~/.local/bin/box-lane -- setsid -w bash $L/ctest_arm.sh default X=1
~/.local/bin/box-lane -- setsid -w bash $L/ctest_arm.sh P1live ${B}GLOBAL=1
~/.local/bin/box-lane -- setsid -w bash $L/ctest_arm.sh faceNSY ${B}MODE=face ${B}NOSAFE=1 ${B}NORUNG=1 ${B}YMAX=1e6
~/.local/bin/box-lane -- setsid -w bash $L/ctest_arm.sh faceNSYfp ${B}MODE=face ${B}NOSAFE=1 ${B}NORELRUNG=1 ${B}YMAX=1e6
echo ALLDONE
