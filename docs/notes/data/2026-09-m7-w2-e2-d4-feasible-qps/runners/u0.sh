#!/usr/bin/env bash
# usage: u0.sh TAG [HVEN_D4_FIX value]  -- the 27 U0 replay cells on the walk engine, one locked leg, then compare_replay vs M6 close
set -o pipefail
D=/home/ghecht/Projects/hven/.scratch/m7w2e2d4; ROOT=/home/ghecht/Projects/hven
tag="$1"; fix="$2"; mkdir -p $D/u0
CELLS="$(grep -v "^#" $ROOT/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt | tr -d "\n")"
export CELLS D tag fix
$D/tools/leg.sh "u0-$tag" bash -c '
  if [ -n "$fix" ]; then export HVEN_D4_FIX="$fix"; fi
  MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 $D/build-bench/bench/hven_sqp_corpus --engine walk --cells "$CELLS" --csv "$D/u0/$tag-walk.csv" > "$D/u0/$tag-walk.log" 2>&1
  echo "corpus rc=$?"'
python3 -B $ROOT/scripts/compare_replay.py --residual-gate 1e-5 $ROOT/docs/notes/data/2026-09-m6-close-gate/replay/head-walk.csv $D/u0/$tag-walk.csv "M6 CLOSE walk vs D4 $tag" > $D/u0/$tag-compare.txt 2>&1; echo "compare rc=$?"; tail -4 $D/u0/$tag-compare.txt
