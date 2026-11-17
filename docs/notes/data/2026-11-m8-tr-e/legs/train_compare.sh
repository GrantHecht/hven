#!/usr/bin/env bash
# usage: train_compare.sh      (no box lock needed: reads CSVs only)      r4 version (supersedes the r1 one, which only had old and new).
# Arms: old = A  (bench 7d974e8 @ hven bdd797e3), mid = A' (bench d7a55b9 + pin edit @ hven 24b2a7e4: OLD lane code, NEW hven), new = B (bench tr-e @ hven 24b2a7e4).
#   (b) A' vs B   : what TR-E's CONSUMER change did, at one hven. THE CLAIM: results 0 mismatches, side files 0 differences (names move, values do not).
#   (a) A  vs B   : the declared old pin vs new pin (hven drift between the pins included). results 0 mismatches expected; side values may move with hven.
#   (c) A  vs A'  : the old lane code at the two pins = hven's drift alone (consumer held fixed).
#   (d) B vs B-traced: tracing moves nothing; every stream validates.
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/tr-e
NEW=$WT/.scratch/bench-new
S=$WT/.scratch/train
EV=$WT/docs/notes/data/2026-11-m8-tr-e/train
mkdir -p "$EV"
fail=0
FILLED=eval_f,eval_g,eval_jac,eval_hess
res() { # tag base fresh [extra compare args]
  local tag=$1 b=$2 f=$3; shift 3
  python3 $NEW/schema/compare.py $b $f "$@" > $EV/results-$tag.txt 2>&1; local rc=$?
  echo "RESULTS $tag rc=$rc: $(tail -1 $EV/results-$tag.txt)"; return $rc; }
side() { # tag base fresh lane [--no-map]
  python3 $NEW/schema/compare_side.py $2 $3 --lane $4 $5 > $EV/side-$1-$4.txt 2>&1; local rc=$?
  echo "SIDE $1 $4 rc=$rc: $(tail -1 $EV/side-$1-$4.txt)"; return $rc; }
for set in hs cutest; do
  # (b) the claim
  res b-$set $S/mid/$set/results.csv $S/new/$set/results.csv --filled $FILLED || fail=1
  for lane in hven-ipm hven-sqp; do side b-$set $S/mid/$set/counters-$lane.csv $S/new/$set/counters-$lane.csv $lane || fail=1; done
  # (a) the pin-spanning comparison and (c) hven's drift alone: reported, and judged below
  res a-$set $S/old/$set/results.csv $S/new/$set/results.csv --filled $FILLED --moved options_digest || fail=1
  res c-$set $S/old/$set/results.csv $S/mid/$set/results.csv --moved options_digest || fail=1
  for lane in hven-ipm hven-sqp; do side a-$set $S/old/$set/counters-$lane.csv $S/new/$set/counters-$lane.csv $lane; side c-$set $S/old/$set/counters-$lane.csv $S/mid/$set/counters-$lane.csv $lane --no-map; done
done
res d-traced $S/new/hs/results.csv $S/new/hs-traced/results.csv --moved traced || fail=1
for lane in hven-ipm hven-sqp; do side d-traced $S/new/hs/counters-$lane.csv $S/new/hs-traced/counters-$lane.csv $lane --no-map || fail=1; done
n=$(find $S/new/hs-traced/traces -name '*.jsonl' | wc -l)
python3 $NEW/schema/trace_v1.py $(find $S/new/hs-traced/traces -name '*.jsonl' | sort) > $EV/streams.txt 2>&1
rc=$?; echo "STREAMS $n streams rc=$rc: $(grep -c ' 0 errors$' $EV/streams.txt) with 0 errors; aborted solves: $(grep -c ', [1-9][0-9]* aborted' $EV/streams.txt)"; [ $rc -eq 0 ] || fail=1
exit $fail
