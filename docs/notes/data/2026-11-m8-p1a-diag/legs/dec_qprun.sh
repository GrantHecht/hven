#!/usr/bin/env bash
# C522 deciders: P1Probe on each dump x arm x algebra; captures the [ p1 ] line and the [orc] oracle line.
# usage (under box-lane): dec_qprun.sh LIST "arm=K:V:K:V ..." OUTFILE
WT=/home/ghecht/Projects/hven/.claude/worktrees/p1a-diag
B=$WT/.scratch/rel/tests/qp/hven_qp_seam_tests
CPUS=$(echo ${BOX_LANE_PHYS:-0 1 2 3} | tr ' ' ',')
one() { # name arm alg rel kv maxit
  local mx=""; [ "$6" != "0" ] && mx="HVEN_P1_MAXIT=$6"
  local out; out=$(env $(echo $5 | tr ':' ' ') $mx HVEN_P1_DUMP=$4 HVEN_P1_ALG=$3 timeout 900 taskset -c $CPUS $B --gtest_filter=P1Probe.RunDump 2>&1)
  local r; r=$(echo "$out" | grep -a '\[ p1 \]' | sed 's/^.*\.txt //; s/ in-cell: .*//')
  local o; o=$(echo "$out" | grep -a '\[orc\]' | sed 's/^.*\[orc\] [^ ]* //')
  local g; g=$(echo "$out" | grep -a '\[d4\]' | tail -1 | sed 's/.*rows/rows/')
  echo "$1 $2 $3: $r | ${o:-no-orc} | ${g}"
}
: > $3
while read -r name rel mx; do for a in $2; do an=${a%%=*}; kv=${a#*=}; [ "$an" = "$a" ] && kv="X=1"
 for alg in ${ALGS:-refactorize border}; do
  one "$name" "$an" "$alg" "$rel" "$kv" "$mx" >> $3 &
  [ $(jobs -r | wc -l) -ge ${BOX_LANE_JOBS:-4} ] && wait -n
 done; done; done < $1
wait
sort -o $3 $3
