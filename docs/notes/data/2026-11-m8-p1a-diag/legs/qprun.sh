#!/usr/bin/env bash
# usage: qprun.sh LIST(name relpath) ARMS(name=K:V..)  -- P1Probe on each dump under each arm (ALGS, default both)
WT=/home/ghecht/Projects/hven/.claude/worktrees/p1a-diag
B=$WT/.scratch/rel/tests/qp/hven_qp_seam_tests
CPUS=$(echo ${BOX_LANE_PHYS:-0 1 2 3} | tr ' ' ',')
one() { # name arm alg rel kv
  local out; out=$(env $(echo $5 | tr ':' ' ') HVEN_P1_TRACE=1 HVEN_P1_DUMP=$4 HVEN_P1_ALG=$3 timeout 900 taskset -c $CPUS $B --gtest_filter=P1Probe.RunDump 2>&1)
  local q; q=$(echo "$out" | grep -a '\[p1q\]' | tail -1 | sed 's/.*upd=/upd=/; s/fprung=0 fpride=0 fpdecl=0 //')
  local r; r=$(echo "$out" | grep -a '\[ p1 \]' | sed 's/^.*txt //; s/ in-cell: result / IC[/; s/hkc=0 maxiter=[0-9]*//; s/arms=0 //')
  echo "$1 $2 $3: $r | $q"
}
while read -r name rel; do for a in $2; do an=${a%%=*}; kv=${a#*=}; [ "$an" = "$a" ] && kv="X=1"
 for alg in ${ALGS:-refactorize border}; do
  one "$name" "$an" "$alg" "$rel" "$kv" &
  [ $(jobs -r | wc -l) -ge 4 ] && wait -n
 done; done; done < $1
wait
