#!/usr/bin/env bash
# sparse-equil v4 r2 (../../DECLARATION-SEQ4-r2.md): copied from ../../legs/ (or ../../runtime/), re-pointed to r2/ and build-k/bin-r2/; older provenance lines below are historical.
# sparse-equil v4: copied from ../../2026-11-m8-kopt-1x-r2/ (same relative path), re-pointed to this worktree and evidence directory; older provenance lines below are historical.
# usage: replay.sh <out-dir> <label> <binary> <reps>   -- the 27-cell corpus replay in walk / ssn / ipm, <reps> times, serial.
#   Run under: flock /tmp/box-build.lock setsid -w bash replay.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/seq4
OUT=${1:?out dir}; LABEL=${2:?label}; BIN=${3:?binary}; REPS=${4:-1}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 TMPDIR=$WT/build-k/tmp
cd "$WT" || exit 1
mkdir -p "$OUT"
echo "binary $BIN sha256 $(sha256sum "$BIN" | cut -d' ' -f1)"
CELLS=$(sed -n 3p docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)
worst=0
for K in $(seq 1 "$REPS"); do
  for MODE in walk ssn ipm; do
    echo "RUN $LABEL k=$K mode=$MODE $(date -Is)"
    "$BIN" --engine "$MODE" --cells "$CELLS" --csv "$OUT/$LABEL-k$K-$MODE.csv" > "$OUT/$LABEL-run-k$K-$MODE.txt" 2>&1
    rc=$?; echo "run rc=$rc"; [ $rc -gt $worst ] && worst=$rc
  done
done
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
