#!/usr/bin/env bash
# AP-2 B2-R1 interior EXPLANATORY diagnosis (after the leg read outside the band; no
# criterion is changed): one perf record per arm on the interior leg, instructions:u,
# flat symbol attribution. Run as: flock /tmp/box-build.lock bash diagnose_interior.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-aba3deeb97313f444
EV=$WT/docs/notes/data/2026-11-m8-ap2-b2/runtime/diagnose-interior
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$EV"
CELLS=$(sed -n 3p docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)
for arm in A B; do
  BIN=$WT/build-b2/arm$arm/hven_sqp_corpus
  env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 perf record -c 200000 -e instructions:u -o "$WT/build-b2/perf-$arm.data" -- \
      "$BIN" --engine interior --cells "$CELLS" --csv "$WT/build-b2/diag-$arm.csv" > /dev/null 2>&1
  echo "record arm=$arm rc=$?"
  perf report -i "$WT/build-b2/perf-$arm.data" --sort symbol --stdio --no-children 2>/dev/null | grep -v '^#' | grep -v '^$' | head -60 > "$EV/top-$arm.txt"
done
echo "FOREGROUND_END $(date -Is)"
