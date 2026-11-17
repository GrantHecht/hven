#!/usr/bin/env bash
# usage: train.sh <old|mid|new>      Run as: flock /tmp/box-build.lock bash train.sh <arm>   (mid = arm A', r4)
# The TRAIN leg for one arm, serial, one cell at a time, alone (CLAUDE.md section 7), TRAIN only (the runner refuses HOLDOUT).
#   hs      : hs-27 TRAIN x hven-ipm, hven-sqp, default labels, budget_wall_s 600
#   cutest  : cutest-nlp-35 TRAIN, five named problems x hven-ipm, hven-sqp, default labels, budget_wall_s 300
#   (new arm only) hs-traced : the hs leg again with --trace, to prove tracing moves no counter and every stream validates
# Raw outputs under .scratch/train/<arm>/ (ignored); summaries are copied into the evidence dir by train_compare.sh.
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/tr-e
ARM=${1:?old|mid|new}
R=$WT/.scratch/bench-$ARM
BIN=$R/build/lanes/hven/hven_bench_cell
OUT=$WT/.scratch/train/$ARM
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 TMPDIR=$WT/.scratch/tmp GIT_CEILING_DIRECTORIES=$WT/.scratch
mkdir -p "$TMPDIR" "$OUT"
cd "$R" || exit 1
git rev-parse HEAD; sha256sum "$BIN"
# run_serial.py exits 1 when ANY cell trips its reference check (hs033/hven-sqp does, at both pins); the results are written
# regardless. A row-writing exit (rc 0 or 1) continues the leg; anything else, or no results.csv, aborts it. (r2: r1 aborted here.)
runs() { # <dir> <args...>
  local d=$1; shift
  python3 runners/run_serial.py --out "$d" "$@" > "$d.log" 2>&1
  local r=$?
  echo "$(basename $d) rc=$r $(date -Is)"; tail -2 "$d.log"
  { [ $r -eq 0 ] || [ $r -eq 1 ]; } && [ -f "$d/results.csv" ] || { echo "RUN_ABORT $d rc=$r"; exit 9; }
}
runs "$OUT/hs" --problem-set hs --split TRAIN --lanes hven-ipm,hven-sqp --budget-wall-s 600 --binary "$BIN"
runs "$OUT/cutest" --set-version 'cutest-nlp-35 v1' --split TRAIN --problems ACOPP57,SMBANK,READING6,ZAMB2-8,DALLASM \
  --lanes hven-ipm,hven-sqp --budget-wall-s 300 --binary "$BIN"
if [ "$ARM" = new ]; then
  runs "$OUT/hs-traced" --problem-set hs --split TRAIN --lanes hven-ipm,hven-sqp --budget-wall-s 600 --binary "$BIN" --trace
fi
echo "FOREGROUND_END $(date -Is)"
exit 0
