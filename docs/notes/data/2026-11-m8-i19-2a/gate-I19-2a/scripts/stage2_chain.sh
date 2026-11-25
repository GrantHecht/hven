#!/usr/bin/env bash
# Stage-2 arms (H0, P, C1; no K and no passing arm) on NLP-46 then PILOT-62.  usage: stage2_chain.sh <store> <arm>...
G=$(cd "$(dirname "$0")" && pwd); STORE=$1; shift
for a in "$@"; do
  bash $G/arm.sh $STORE H $a NLP-46 all > $G/../arms/H-$a-nlp.log 2>&1; rc=$?
  echo "$(date -Is) arm $a NLP-46 rc=$rc"; [ $rc -eq 0 ] || { echo "CHAIN STOPPED at $a"; exit $rc; }
done
for a in "$@"; do
  [ $a = H0 ] && continue
  bash $G/pilot.sh $STORE H $a > $G/../arms/H-$a-pilot.log 2>&1; rc=$?
  echo "$(date -Is) pilot $a rc=$rc"; [ $rc -eq 0 ] || { echo "CHAIN STOPPED at pilot $a"; exit $rc; }
done
echo CHAIN2_DONE
