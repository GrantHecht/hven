#!/usr/bin/env bash
# usage: examples.sh <dir under build-pfe/>: every hven_example_* runs and exits 0
source "$(dirname "$0")/common.sh"
worst=0; n=0
for EX in $(find "$BP/${1:?}/examples" -maxdepth 1 -type f -executable -name 'hven_example_*' | sort); do
  echo "=== $(basename "$EX")"; "$EX" 2>&1 | tail -4; rc=${PIPESTATUS[0]}; echo "rc=$rc"; n=$((n+1))
  [ $rc -gt $worst ] && worst=$rc
done
echo "examples=$n worst=$worst"
[ "$n" -eq 8 ] || worst=1
fin $worst
