#!/usr/bin/env bash
# PF-4 r2 (../../DECLARATION-PF-4-r2.md): copy of ../../legs/replay.sh re-pointed to the pf4-r2 worktree and r2/ (paths only); older lines are historical.
# PF-4 P2: ../../2026-11-m8-wa1/legs/replay.sh re-pointed. The 27-cell corpus replay in walk / ssn / ipm, <reps> times, serial.
# usage: replay.sh <out-dir> <label> <binary> <reps>   Run under (r2): ~/.local/bin/box-lane -- bash replay.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf4-r2
OUT=${1:?out dir}; LABEL=${2:?label}; BIN=${3:?binary}; REPS=${4:-1}
[ -n "${BOX_LANE:-}" ] || { echo "NOT_IN_BOX_LANE (r2: every leg runs inside ~/.local/bin/box-lane)"; exit 3; }; echo "BOX_LANE=$BOX_LANE phys=[$BOX_LANE_PHYS] jobs=$BOX_LANE_JOBS"
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 TMPDIR=$WT/.scratch/tmp
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
