#!/usr/bin/env bash
# usage: run.sh TAG PROBLEM LABEL [env...] -- one instrumented cell as its own locked leg (QP dumps on), then a summary line
set -o pipefail
D=/home/ghecht/Projects/hven/.scratch/m7w2e2d4
tag="$1"; prob="$2"; label="$3"; shift 3
mkdir -p $D/dumps/$tag
$D/tools/leg.sh "$tag" $D/tools/cell.sh $D/runs/$tag "$prob" "$label" HVEN_D3=1 HVEN_D4_QPDUMP=$D/dumps/$tag "$@" >> $D/logs/legs.log 2>&1
rc=$?
o=$D/runs/$tag.out
printf 'RUN %s rc=%s %s %s %s %s restoration_iters=%s elastic=%s dumps=%s\n' "$tag" "$rc" "$(grep -m1 '^status=' $o)" "$(grep -m1 '^iterations=' $o)" "$(grep -m1 '^objective=' $o)" "$(grep -m1 '^factorizations=' $o)" "$(grep -m1 'side.restoration_iters=' $o | cut -d= -f2)" "$(grep -m1 'side.elastic_activations=' $o | cut -d= -f2)" "$(ls $D/dumps/$tag | wc -l)" | tee -a $D/logs/summary.txt
exit $rc
