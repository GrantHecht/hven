#!/usr/bin/env bash
cd /home/ghecht/Projects/hven/.claude/worktrees/p1a-diag
B=.scratch/rel/tests/qp/hven_qp_seam_tests
C="HVEN_P1_DUMP=PF1_f7_n1000_path_corrupted_qp1.txt HVEN_P1_ALG=refactorize HVEN_D3_ROW=905 HVEN_D3_MAX=100000"
env $C HVEN_P1_MAXIT=6000 timeout 900 $B --gtest_filter=P1Probe.RunDump > .scratch/out/dec/d3_base.txt 2>&1 &
env $C HVEN_P1_MAXIT=6000 HVEN_P1_MODE=face HVEN_P1_SAFEBOOT=1 HVEN_P1_NORELRUNG=1 HVEN_P1_YMAX=1e6 timeout 900 $B --gtest_filter=P1Probe.RunDump > .scratch/out/dec/d3_F.txt 2>&1 &
env $C HVEN_P1_MAXIT=6000 HVEN_D2=1 HVEN_P1_NORELRUNG=1 timeout 900 $B --gtest_filter=P1Probe.RunDump > .scratch/out/dec/d3_D2.txt 2>&1 &
wait
