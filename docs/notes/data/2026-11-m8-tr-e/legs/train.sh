#!/usr/bin/env bash
# usage: train.sh <old|new>      Run as: flock /tmp/box-build.lock bash train.sh <arm>
# The TRAIN leg for one arm, serial, one cell at a time, alone (CLAUDE.md section 7), TRAIN only (the runner refuses HOLDOUT).
#   hs      : hs-27 TRAIN x hven-ipm, hven-sqp, default labels, budget_wall_s 600
#   cutest  : cutest-nlp-35 TRAIN, five named problems x hven-ipm, hven-sqp, default labels, budget_wall_s 300
#   (new arm only) hs-traced : the hs leg again with --trace, to prove tracing moves no counter and every stream validates
# Raw outputs under .scratch/train/<arm>/ (ignored); summaries are copied into the evidence dir by train_compare.sh.
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/tr-e
ARM=${1:?old|new}
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
rc=0
python3 runners/run_serial.py --out "$OUT/hs" --problem-set hs --split TRAIN --lanes hven-ipm,hven-sqp \
  --budget-wall-s 600 --binary "$BIN" > "$OUT/hs.log" 2>&1 || rc=$?
echo "hs rc=$rc $(date -Is)"; tail -2 "$OUT/hs.log"
[ $rc -eq 0 ] || exit $rc
python3 runners/run_serial.py --out "$OUT/cutest" --set-version 'cutest-nlp-35 v1' --split TRAIN \
  --problems ACOPP57,SMBANK,READING6,ZAMB2-8,DALLASM --lanes hven-ipm,hven-sqp \
  --budget-wall-s 300 --binary "$BIN" > "$OUT/cutest.log" 2>&1 || rc=$?
echo "cutest rc=$rc $(date -Is)"; tail -2 "$OUT/cutest.log"
[ $rc -eq 0 ] || exit $rc
if [ "$ARM" = new ]; then
  python3 runners/run_serial.py --out "$OUT/hs-traced" --problem-set hs --split TRAIN --lanes hven-ipm,hven-sqp \
    --budget-wall-s 600 --binary "$BIN" --trace > "$OUT/hs-traced.log" 2>&1 || rc=$?
  echo "hs-traced rc=$rc $(date -Is)"; tail -2 "$OUT/hs-traced.log"
fi
echo "FOREGROUND_END $(date -Is)"
exit $rc
