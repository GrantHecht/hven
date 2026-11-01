#!/usr/bin/env bash
# usage: oracle_all.sh LABEL OUTFILE < list-of-dumps  -- the python oracle on each dump, ONE locked leg, single-threaded BLAS
set -o pipefail
D=/home/ghecht/Projects/hven/.scratch/m7w2e2d4
PY=/tmp/claude-1000/-home-ghecht-Projects-hven/0c17d1d8-660f-4914-9feb-e49a88005c27/scratchpad/venv/bin/python
label="$1"; out="$2"
export LIST="$(cat)" PY
$D/tools/leg.sh "oracle-$label" bash -c '
for f in $LIST; do OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 MKL_NUM_THREADS=1 taskset -c 2 $PY /home/ghecht/Projects/hven/.scratch/m7w2e2d4/tools/qp_oracle.py $f --json ${f%.txt}.oracle.json 2>&1; done
' > "$out" 2>&1
