#!/usr/bin/env bash
# usage: replay_leg.sh LABEL OUTDIR DUMP... -- ONE locked leg: each dump replayed (E2 on, traced), (E2 off, traced),
# then the python oracle; all single-threaded, taskset -c 2.
set -o pipefail
R=/home/ghecht/Projects/hven/.scratch/m7w2r2
PY=/tmp/claude-1000/-home-ghecht-Projects-hven/0c17d1d8-660f-4914-9feb-e49a88005c27/scratchpad/venv/bin/python
label="$1"; outdir="$2"; shift 2; mkdir -p "$outdir"
export LIST="$*" PY R outdir
$R/tools/leg.sh "replay-$label" bash -c '
for f in $LIST; do
  cell=$(basename $(dirname $f)); q=$(basename $f .txt); b=$outdir/${cell}_${q}
  env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 HVEN_D4_TRACE=1 taskset -c 2 $R/replay-build/qp_replay $f --out $b.on.sol > $b.on.res 2> $b.on.trace
  echo "$cell $q E2on  $(head -1 $b.on.res)"
  env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 HVEN_D4_TRACE=1 taskset -c 2 $R/replay-build/qp_replay $f --e2 0 --out $b.off.sol > $b.off.res 2> $b.off.trace
  echo "$cell $q E2off $(head -1 $b.off.res)"
  OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 MKL_NUM_THREADS=1 taskset -c 2 $PY $R/tools/qp_oracle.py $f --x $b.on.sol --json $b.oracle.json > $b.oracle.txt 2>&1
  echo "$cell $q oracle rc=$?"
done'
