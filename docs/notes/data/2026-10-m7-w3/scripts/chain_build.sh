#!/bin/bash
# W3 build chain: every build its own leg under the box lock.
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/w3
S=$W/.scratch/w3
LEG=$S/scripts/leg.sh; B=$S/scripts/build.sh
SUM=$S/logs/chain-build.txt
P="hven_sqp_corpus hven_sqp_ssn_safeguard_probe hven_sqp_tau_bar_sweep_probe"
echo "BUILD CHAIN START $(date -Is) pid=$$" | tee -a $SUM
run() { local n=$1; shift; $LEG "$n" "$@" >> $SUM 2>&1; echo "$n rc=$? $(date -Is)" | tee -a $SUM; }
run b-head-rel   $B $W $S/build-head-rel Release
run b-base-rel   $B $S/src-base $S/build-base-rel Release hven_sqp_corpus
run b-farkas-rel $B $S/src-farkas1e-4 $S/build-farkas-rel Release $P
run b-tau10-rel  $B $S/src-tau10 $S/build-tau10-rel Release $P
run b-tau1e3-rel $B $S/src-tau1e3 $S/build-tau1e3-rel Release $P
run b-head-dbg   $B $W $S/build-head-dbg Debug hven_sqp_corpus hven_sqp_ssn_safeguard_probe hven_sqp_tau_bar_sweep_probe hven_sqp_ipqp_e1_arm hven_sqp_bench hven_sqp_f7_cold hven_sqp_crossover
echo "BUILD CHAIN END $(date -Is)" | tee -a $SUM
