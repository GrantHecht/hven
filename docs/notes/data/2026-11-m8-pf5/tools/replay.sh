#!/usr/bin/env bash
# PF-5 prototype replay: 27 cells x walk/ssn/ipm, arm per HVEN_PF5_RETAIN, serial, MKL_NUM_THREADS=1.
# usage: replay.sh <outdir> <label> <retain>   Run under: setsid -w flock /tmp/box-build.lock
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf5-proto
OUT=${1:?}; LABEL=${2:?}; RET=${3:?}
BIN=$WT/.scratch/rel/bench/hven_sqp_corpus
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 TMPDIR=$WT/.scratch/tmp
unset HVEN_PF5_RETAIN; [ "$RET" != 0 ] && export HVEN_PF5_RETAIN=$RET
cd "$WT"; mkdir -p "$OUT"; echo "FOREGROUND_START $(date -Is) $(sha256sum $BIN)"
CELLS=$(sed -n 3p docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)
for MODE in walk ssn ipm; do
  taskset -c ${BOX_LANE_PHYS%% *} "$BIN" --engine "$MODE" --cells "$CELLS" --csv "$OUT/$LABEL-$MODE.csv" > "$OUT/$LABEL-run-$MODE.txt" 2>&1; echo "$MODE rc=$?"
done
echo "FOREGROUND_END $(date -Is)"
