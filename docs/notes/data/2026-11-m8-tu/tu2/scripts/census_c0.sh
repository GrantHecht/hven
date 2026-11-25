#!/bin/bash
cd /home/ghecht/Projects/hven-bench-tu2
while ! grep -q "ARM A5 DONE\|FAILED rc=1$" <(tail -1 .scratch/logs/driver.log); do sleep 30; done
tail -1 .scratch/logs/driver.log | grep -q DONE || exit 1
W=/home/ghecht/Projects/hven/.claude/worktrees/tu2
mkdir -p .scratch/census/C0
~/.local/bin/box-lane -- bash .scratch/run_walk_census_lane.sh --binary $PWD/.scratch/bin/hven_sqp_corpus.pin --out $PWD/.scratch/census/C0 \
  --baseline $W/bench/baselines/2026-10-m7-close-corpus/walk_baseline.csv --compare $W/scripts/census_compare.py --t1-width 4 --t3-width 4 > .scratch/logs/census-C0.log 2>&1
echo "C0 rc=$?" >> .scratch/logs/census-C0.log
