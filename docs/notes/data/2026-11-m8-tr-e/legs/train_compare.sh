#!/usr/bin/env bash
# usage: train_compare.sh      (no box lock needed: reads CSVs only)
# Compares the OLD-pin TRAIN leg with the NEW-pin one through the declared maps, and writes the summaries into
# the evidence dir train/. Verdict lines: TRAIN-RESULTS (hs, cutest), TRAIN-SIDE (4 side files), TRAIN-TRACED (traced vs untraced, new arm),
# TRAIN-STREAMS (every stream validated by schema/trace_v1.py). Exit 0 iff all are zero-difference.
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/tr-e
NEW=$WT/.scratch/bench-new
S=$WT/.scratch/train
EV=$WT/docs/notes/data/2026-11-m8-tr-e/train
mkdir -p "$EV"
fail=0
FILLED=eval_f,eval_g,eval_jac,eval_hess
for set in hs cutest; do
  python3 $NEW/schema/compare.py $S/old/$set/results.csv $S/new/$set/results.csv --filled $FILLED --moved options_digest > $EV/results-$set.txt 2>&1
  rc=$?; echo "TRAIN-RESULTS $set rc=$rc: $(tail -1 $EV/results-$set.txt)"; [ $rc -eq 0 ] || fail=1
  for lane in hven-ipm hven-sqp; do
    python3 $NEW/schema/compare_side.py $S/old/$set/counters-$lane.csv $S/new/$set/counters-$lane.csv --lane $lane > $EV/side-$set-$lane.txt 2>&1
    rc=$?; echo "TRAIN-SIDE $set $lane rc=$rc: $(tail -1 $EV/side-$set-$lane.txt)"; [ $rc -eq 0 ] || fail=1
  done
done
python3 $NEW/schema/compare.py $S/new/hs/results.csv $S/new/hs-traced/results.csv --moved traced > $EV/results-traced.txt 2>&1
rc=$?; echo "TRAIN-TRACED rc=$rc: $(tail -1 $EV/results-traced.txt)"; [ $rc -eq 0 ] || fail=1
for lane in hven-ipm hven-sqp; do
  python3 $NEW/schema/compare_side.py $S/new/hs/counters-$lane.csv $S/new/hs-traced/counters-$lane.csv --lane $lane > $EV/side-traced-$lane.txt 2>&1
  rc=$?; echo "TRAIN-TRACED-SIDE $lane rc=$rc: $(tail -1 $EV/side-traced-$lane.txt)"; [ $rc -eq 0 ] || fail=1
done
n=$(find $S/new/hs-traced/traces -name '*.jsonl' | wc -l)
python3 $NEW/schema/trace_v1.py $(find $S/new/hs-traced/traces -name '*.jsonl' | sort) > $EV/streams.txt 2>&1
rc=$?; echo "TRAIN-STREAMS $n streams rc=$rc: $(grep -c ' 0 errors$' $EV/streams.txt) with 0 errors, $(grep -c 'aborted' $EV/streams.txt) lines mention aborted"; [ $rc -eq 0 ] || fail=1
exit $fail
