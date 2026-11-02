#!/usr/bin/env bash
# AP-2 B2-R2 (INFORMATIONAL, no band; DECLARATION.txt): the public route's per-solve
# re-read cost. B2's hven_sqp_corpus (build-b2/armB), arm A = --internal-route
# nlp-internal, arm B = --internal-route nlp-public, one round, the ten dual-binding
# cells (AMENDMENT-B2a.txt), the same perf events and pinning as B2-R1.
# usage: runtime_r2.sh <mode: walk|interior> <out-dir>    (under the wrapper's flock)
set -o pipefail
MODE=${1:?walk|interior}
OUT=${2:?out dir}
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-aba3deeb97313f444
cd "$WT" || exit 1
BIN=$WT/build-b2/armB/hven_sqp_corpus
CORE=2
EVENTS=instructions:u,branches:u,cycles:u,branch-misses:u,L1-icache-load-misses:u
CELLS=f7_n1000_bound_neutral,f7_n1000_bound_physics,f7_n2000_bound_neutral,f7_n2000_bound_physics,f7_n5000_bound_neutral,f7_n5000_bound_physics,f7_n10000_bound_neutral,f7_n10000_bound_physics,f7_n20000_bound_neutral,f7_n20000_bound_physics
mkdir -p "$OUT/perf" "$OUT/raw"
worst=0
route_of() { [ "$1" = A ] && echo nlp-internal || echo nlp-public; }
echo "LEG_START mode=$MODE $(date -Is)"
sha256sum "$BIN"
if [ "$MODE" = interior ]; then
  for arm in A B; do
    env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c $CORE "$BIN" --engine interior --internal-route "$(route_of $arm)" --cells "$CELLS" --csv "$OUT/raw/warmup-$arm.csv" > /dev/null 2>&1
  done
  for arm in A B; do
    echo "RUN mode=$MODE arm=$arm $(date -Is)"
    env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c $CORE \
        perf stat -x, -e $EVENTS -o "$OUT/perf/$MODE-r1-$arm-all.txt" -- \
        "$BIN" --engine interior --internal-route "$(route_of $arm)" --cells "$CELLS" --csv "$OUT/raw/$MODE-r1-$arm-all.csv" \
        > "$OUT/raw/$MODE-r1-$arm-all.stdout" 2>&1
    rc=$?; [ $rc -gt $worst ] && worst=$rc
  done
else
  first=$(echo "$CELLS" | cut -d, -f1)
  for arm in A B; do
    env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c $CORE "$BIN" --internal-run-one "$first" --engine "$MODE" --internal-route "$(route_of $arm)" --internal-out "$OUT/raw/warmup-$arm.csv" > /dev/null 2>&1
  done
  for cell in $(echo "$CELLS" | tr ',' ' '); do
    for arm in A B; do
      echo "RUN mode=$MODE cell=$cell arm=$arm $(date -Is)"
      env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c $CORE \
          perf stat -x, -e $EVENTS -o "$OUT/perf/$MODE-r1-$arm-$cell.txt" -- \
          "$BIN" --internal-run-one "$cell" --engine "$MODE" --internal-route "$(route_of $arm)" --internal-out "$OUT/raw/$MODE-r1-$arm-$cell.csv" \
          > "$OUT/raw/$MODE-r1-$arm-$cell.csv.stdout" 2> "$OUT/raw/$MODE-r1-$arm-$cell.csv.stderr"
      rc=$?; [ $rc -gt $worst ] && worst=$rc
    done
  done
fi
echo "LEG_END mode=$MODE worst_rc=$worst $(date -Is)"
exit $worst
