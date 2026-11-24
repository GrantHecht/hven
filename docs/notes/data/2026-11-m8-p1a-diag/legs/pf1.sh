#!/usr/bin/env bash
# PF1 probe across arms: both algebras. usage: pf1.sh <dumpfile rel e2-d4> <maxit>
WT=/home/ghecht/Projects/hven/.claude/worktrees/p1a-diag
B=$WT/.scratch/rel/tests/qp/hven_qp_seam_tests
F=${1:-PF1_f7_n1000_path_corrupted_qp1.txt}; M=${2:-20000}
run() { local arm="$1"; shift
  for alg in refactorize border; do
    ( env "$@" HVEN_P1_TRACE=1 HVEN_P1_DUMP=$F HVEN_P1_MAXIT=$M HVEN_P1_ALG=$alg timeout 900 taskset -c $CPUS $B --gtest_filter=P1Probe.RunDump 2>&1 | grep -a '\[ p1 \]\|\[p1q\]' | tail -2 | sed "s/^/$arm $alg: /" | cut -c1-330 ) &
    [ $(jobs -r | wc -l) -ge 4 ] && wait -n
  done
}
CPUS=$(echo ${BOX_LANE_PHYS:-0 1 2 3} | tr ' ' ',')
for a in ${ARMS}; do run $a $(echo $a | tr ':' ' '); done
wait
