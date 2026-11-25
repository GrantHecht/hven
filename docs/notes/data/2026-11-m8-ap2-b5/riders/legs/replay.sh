#!/usr/bin/env bash
# AP-2 B5 riders (B5's instrument): the 27-cell replay x walk/ssn/ipm on the default (internal) route,
# compared EXACT against docs/notes/data/2026-10-m7-wx2-gates/gates/replay/head-*.csv.
# usage: replay.sh <out-dir>      Run as: flock /tmp/box-build.lock bash replay.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a17ce5e51d9c90f96
OUT=${1:?out dir}
BIN=$WT/build-b5/rel/bench/hven_sqp_corpus
REF=$WT/docs/notes/data/2026-10-m7-wx2-gates/gates/replay
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
cd "$WT" || exit 1
mkdir -p "$OUT"
sha256sum "$BIN"
CELLS=$(sed -n 3p docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)
worst=0
for MODE in walk ssn ipm; do
  echo "RUN mode=$MODE $(date -Is)"
  "$BIN" --engine "$MODE" --cells "$CELLS" --csv "$OUT/riders-$MODE.csv" > "$OUT/riders-run-$MODE.txt" 2>&1
  rc=$?; echo "run rc=$rc"; [ $rc -gt $worst ] && worst=$rc
  python3 scripts/compare_replay.py "$REF/head-$MODE.csv" "$OUT/riders-$MODE.csv" "riders-$MODE" > "$OUT/riders-comparison-$MODE.txt" 2>&1
  rc=$?; echo "compare rc=$rc"; tail -3 "$OUT/riders-comparison-$MODE.txt"; [ $rc -gt $worst ] && worst=$rc
done
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
