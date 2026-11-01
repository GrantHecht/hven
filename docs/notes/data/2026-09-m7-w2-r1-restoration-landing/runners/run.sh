#!/usr/bin/env bash
# usage: run.sh TAG PROBLEM LABEL [env...] -- one instrumented cell as its own locked leg, then a summary line
set -o pipefail
R=/home/ghecht/Projects/hven/.scratch/m7w2r1
tag="$1"; prob="$2"; label="$3"; shift 3
mkdir -p $R/runs
$R/tools/leg.sh "$tag" $R/tools/cell.sh $R/runs/$tag "$prob" "$label" HVEN_R1=1 "$@" >> $R/logs/legs.log 2>&1
rc=$?
o=$R/runs/$tag.out
printf 'RUN %s rc=%s %s %s %s %s resto=%s %s\n' "$tag" "$rc" "$(grep -m1 '^status=' $o)" "$(grep -m1 '^iterations=' $o)" "$(grep -m1 '^objective=' $o)" "$(grep -m1 '^factorizations=' $o)" "$(grep -m1 'restoration_iters=' $o | cut -d= -f2)" "$(grep "^cell " $R/logs/legs.log | tail -1 | grep -o "wall_s=.*")" | tee -a $R/logs/summary.txt
exit $rc
