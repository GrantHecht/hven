#!/usr/bin/env bash
WT=/home/ghecht/Projects/hven/.claude/worktrees/p1a-diag; L=$WT/.scratch/legs; B=HVEN_P1_
~/.local/bin/box-lane -- setsid -w bash $L/ctest_arm.sh faceSBfp ${B}MODE=face ${B}SAFEBOOT=1 ${B}NORELRUNG=1 ${B}YMAX=1e6
~/.local/bin/box-lane -- setsid -w bash $L/ctest_arm.sh faceSB ${B}MODE=face ${B}SAFEBOOT=1 ${B}NORUNG=1 ${B}YMAX=1e6
echo ALLDONE
