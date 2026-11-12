#!/usr/bin/env bash
# M8 DN-3a (../DECLARATION.txt): copied from ../../2026-11-m8-ipm-nonum-restore/legs/ and re-pointed in code lines only (worktree agent-acfc8f7d568381148, build-dn3a/, this directory, arm A b70e45cfb, replay outputs dn3a-*); older provenance lines below are historical.
# M8 IPM no-numerics restoration routing (../DECLARATION.txt): copied from ../../2026-11-m8-ipm-ladder-status/ (legs/, runtime/) and re-pointed in code lines only (worktree agent-a3d6ec088fa738122, build-dn3a/, this directory, arm A b9626e7e5, replay outputs dn3a-*); older provenance lines below are historical.
# M8 IPM ladder status (../DECLARATION.txt): copied from ../../2026-11-m8-nan-gates/ (legs/, runtime-r2/ -> runtime/) and re-pointed in code lines only (worktree ipmlad, build-ipmlad/, this directory, arm A); older provenance lines below are historical.
# M8 NaN-sticky gates: copied from ../../2026-11-m8-dn1d-singular/r3/legs/ and re-pointed (worktree nan, build-nan/, this directory).
# usage: replay.sh <out-dir>      Run as: flock /tmp/box-build.lock bash replay.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-acfc8f7d568381148
OUT=${1:?out dir}
BIN=$WT/build-dn3a/rel/bench/hven_sqp_corpus
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
  "$BIN" --engine "$MODE" --cells "$CELLS" --csv "$OUT/dn3a-$MODE.csv" > "$OUT/dn3a-run-$MODE.txt" 2>&1
  rc=$?; echo "run rc=$rc"; [ $rc -gt $worst ] && worst=$rc
  python3 scripts/compare_replay.py "$REF/head-$MODE.csv" "$OUT/dn3a-$MODE.csv" "dn3a-$MODE" > "$OUT/dn3a-comparison-$MODE.txt" 2>&1
  rc=$?; echo "compare rc=$rc"; tail -3 "$OUT/dn3a-comparison-$MODE.txt"; [ $rc -gt $worst ] && worst=$rc
done
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
