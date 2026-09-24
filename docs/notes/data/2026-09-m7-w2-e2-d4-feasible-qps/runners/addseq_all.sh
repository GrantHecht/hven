#!/usr/bin/env bash
# one locked leg: addseq --stop-first on every baseline trace
D=/home/ghecht/Projects/hven/.scratch/m7w2e2d4
PY=/tmp/claude-1000/-home-ghecht-Projects-hven/0c17d1d8-660f-4914-9feb-e49a88005c27/scratchpad/venv/bin/python
export PY D
$D/tools/leg.sh addseq-all bash -c '
for f in $D/dumps/*/qp*.txt; do c=$(basename $(dirname $f)); q=$(basename $f .txt); t=$D/replays/base/${c}_${q}.trace
  OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 taskset -c 2 $PY $D/tools/addseq.py $f $t --stop-first --max-it 1500 > $D/replays/base/${c}_${q}.addseq 2>&1
  echo "$c $q adds=$(grep -c ADD $D/replays/base/${c}_${q}.addseq) first_exact: $(grep EXACT-DEP $D/replays/base/${c}_${q}.addseq | grep -v exch | head -1)"
done' 
