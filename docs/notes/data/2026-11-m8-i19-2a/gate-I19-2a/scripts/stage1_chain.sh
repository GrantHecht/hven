#!/usr/bin/env bash
# Stage-1 arms on the H lane, in a fixed order (G6 first so N3 can be read early). usage: stage1_chain.sh <store> <arm>...
G=$(cd "$(dirname "$0")" && pwd); STORE=$1; shift
for a in "$@"; do
  bash $G/arm.sh $STORE H $a TRAIN-238,CAP-238-1,CAP-238-01,SENS-P all > $G/../arms/H-$a.log 2>&1; rc=$?
  echo "$(date -Is) arm $a rc=$rc"
  [ $rc -eq 0 ] || { echo "CHAIN STOPPED at $a rc=$rc"; exit $rc; }
done
echo CHAIN_DONE
