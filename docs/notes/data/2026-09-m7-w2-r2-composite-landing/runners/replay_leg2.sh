#!/usr/bin/env bash
# usage: replay_leg2.sh LABEL OUTDIR DUMP... -- ONE locked leg: each dump replayed on 4 configs
# (as dumped algebra: E2 on / off; refactorize: E2 on / off), traced, then the python oracle; single-threaded, taskset -c 2.
set -o pipefail
R=/home/ghecht/Projects/hven/.scratch/m7w2r2
PY=/tmp/claude-1000/-home-ghecht-Projects-hven/0c17d1d8-660f-4914-9feb-e49a88005c27/scratchpad/venv/bin/python
label="$1"; outdir="$2"; shift 2; mkdir -p "$outdir"
export LIST="$*" PY R outdir
$R/tools/leg.sh "replay-$label" bash -c '
for f in $LIST; do
  cell=$(basename $(dirname $f)); q=$(basename $f .txt); b=$outdir/${cell}_${q}
  for cfg in "on:--e2 1" "off:--e2 0" "ron:--e2 1 --algebra refactorize" "roff:--e2 0 --algebra refactorize"; do
    t=${cfg%%:*}; a=${cfg#*:}
    env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 HVEN_D4_TRACE=1 taskset -c 2 $R/replay-build/qp_replay $f $a --out $b.$t.sol > $b.$t.res 2> $b.$t.trace
    echo "$cell $q $t $(head -1 $b.$t.res)"
  done
  OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 MKL_NUM_THREADS=1 taskset -c 2 $PY $R/tools/qp_oracle.py $f --x $b.on.sol --json $b.oracle.json > $b.oracle.txt 2>&1
  echo "$cell $q oracle rc=$?"
done'
