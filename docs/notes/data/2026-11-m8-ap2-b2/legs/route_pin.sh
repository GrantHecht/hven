#!/usr/bin/env bash
# AP-2 B2-a: the public Problem(Nlp) route against today's triplet route, on the ten
# dual-binding replay cells, walk/ssn/ipm (SQP) and interior (IPM). DECLARATION.txt B2-a
# as amended by AMENDMENT-B2a.txt.
# usage: route_pin.sh <out-dir>      Run as: flock /tmp/box-build.lock bash route_pin.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-aba3deeb97313f444
OUT=${1:?out dir}
BIN=$WT/build-b2/rel/bench/hven_sqp_corpus
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
cd "$WT" || exit 1
mkdir -p "$OUT"
sha256sum "$BIN"
CELLS=f7_n1000_bound_neutral,f7_n1000_bound_physics,f7_n2000_bound_neutral,f7_n2000_bound_physics,f7_n5000_bound_neutral,f7_n5000_bound_physics,f7_n10000_bound_neutral,f7_n10000_bound_physics,f7_n20000_bound_neutral,f7_n20000_bound_physics
worst=0
for MODE in walk ssn ipm interior; do
  for ROUTE in nlp-internal nlp-public; do
    echo "RUN mode=$MODE route=$ROUTE $(date -Is)"
    "$BIN" --engine "$MODE" --internal-route "$ROUTE" --cells "$CELLS" --csv "$OUT/$MODE-$ROUTE.csv" > "$OUT/run-$MODE-$ROUTE.txt" 2>&1
    rc=$?; echo "run rc=$rc"; [ $rc -gt $worst ] && worst=$rc
  done
  python3 scripts/compare_replay.py "$OUT/$MODE-nlp-internal.csv" "$OUT/$MODE-nlp-public.csv" "b2a-$MODE" > "$OUT/comparison-$MODE-exact.txt" 2>&1
  rc=$?; echo "compare-exact mode=$MODE rc=$rc"; tail -3 "$OUT/comparison-$MODE-exact.txt"
  if [ $rc -ne 0 ]; then
    # The declared secondary reading (DECLARATION.txt B2-a), reported beside the exact one.
    python3 scripts/compare_replay.py --residual-gate 1e-5 "$OUT/$MODE-nlp-internal.csv" "$OUT/$MODE-nlp-public.csv" "b2a-$MODE-gate" > "$OUT/comparison-$MODE-gate.txt" 2>&1
    rc2=$?; echo "compare-gate mode=$MODE rc=$rc2"; tail -3 "$OUT/comparison-$MODE-gate.txt"
  fi
done
echo "worst_run_rc=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
